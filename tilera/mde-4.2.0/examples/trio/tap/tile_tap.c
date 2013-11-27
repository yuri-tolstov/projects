// Copyright 2013 Tilera Corporation. All Rights Reserved.
//
//   The source code contained or described herein and all documents
//   related to the source code ("Material") are owned by Tilera
//   Corporation or its suppliers or licensors.  Title to the Material
//   remains with Tilera Corporation or its suppliers and licensors. The
//   software is licensed under the Tilera MDE License.
//
//   Unless otherwise agreed by Tilera in writing, you may not remove or
//   alter this notice or any other notice embedded in Materials by Tilera
//   or Tilera's suppliers or licensors in any way.
//
//

#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>

#include <gxio/trio.h>
#include <gxpci/gxpci.h>
#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/perf.h>
#include <tmc/task.h>
#include <tmc/spin.h>

#define die(...)                tmc_task_die(__VA_ARGS__)
#define debug(...)

#define NUM_THREADS		2
#define MIN_PKT_SIZE		32
#define MAX_PKT_SIZE		(4 * 1024)
#define MAX_PKTS                GXPCI_HOST_PQ_SEGMENT_ENTRIES
#define MAP_LENGTH 		(MAX_PKTS * MAX_PKT_SIZE)

static int trio_index = 0;      // TRIO instance index.
static int queue_index = 0;     // Queue index for both sender and receiver.
static int vf_instance = -1;    // Virtual Function (SR-IOV)
static int mac_index;           // Local MAC index.
static int cpu_rank = 1;        // CPU affinity.
static int tap_fd = -1;         // TAP device descriptor.
static int ctl_fd;              // Control FD for interface operations.
static int restart = 1;         // Phoenix mode - restart on death.

static gxpci_context_t h2t_context_body;
static gxpci_context_t *h2t_context = &h2t_context_body;
static gxpci_queue_type_t h2t_type = GXPCI_PQ_H2T;

static gxpci_context_t t2h_context_body;
static gxpci_context_t *t2h_context = &t2h_context_body;
static gxpci_queue_type_t t2h_type = GXPCI_PQ_T2H;

static pthread_t threads[NUM_THREADS];
static tmc_spin_barrier_t barrier;
static cpu_set_t desired_cpus;

static const char *if_name = NULL;
static const char *if_addr = NULL;

static pthread_mutex_t tap_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned send_read, send_post, send_comp;
static unsigned recv_write, recv_post, recv_comp;

typedef struct {
	uint32_t	size;
	uint32_t	pad[3];
} tile_hdr_t;

static char *shift_option(char ***arglist, const char* option)
{
  char** args = *arglist;
  char* arg = args[0], **rest = &args[1];
  int optlen = strlen(option);
  char* val = arg + optlen;
  if (option[optlen - 1] != '=')
  {
    if (strcmp(arg, option))
      return NULL;
  }
  else
  {
    if (strncmp(arg, option, optlen - 1))
      return NULL;
    if (arg[optlen - 1] == '\0')
      val = *rest++;
    else if (arg[optlen - 1] != '=')
      return NULL;
  }
  *arglist = rest;
  return val;
}

// Parses command line arguments in order to fill in the MAC and bus
// address variables.
static void parse_args(int argc, char** argv)
{
  char **args = &argv[1];

  // Scan options.
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--mac=")))
      mac_index = atoi(opt);
    else if ((opt = shift_option(&args, "--cpu-rank=")))
      cpu_rank = atoi(opt);
    else if ((opt = shift_option(&args, "--if-name=")))
      if_name = opt;
    else if ((opt = shift_option(&args, "--if-addr=")))
      if_addr = opt;
    else if ((opt = shift_option(&args, "--queue=")))
      queue_index = atoi(opt);
    else if ((opt = shift_option(&args, "--vf="))) {
      t2h_type = GXPCI_PQ_T2H_VF;
      h2t_type = GXPCI_PQ_H2T_VF;
      vf_instance = atoi(opt);
    } else if ((opt = shift_option(&args, "--no-restart")))
      restart = 0;
    else
    {
      die("unknown option '%s'.\n", args[0]);
    }
  }
}

static void init_ifreq(struct ifreq *ifr)
{
  memset(ifr, 0, sizeof(ifr));
  if (if_name) {
    strncpy(ifr->ifr_name, if_name, sizeof(ifr->ifr_name));
  } else if (vf_instance == -1) {
    snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "gxpci%d-%d",
             trio_index + 1, queue_index);
  } else {
    snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "gxpci%d-vf%d-%d",
             trio_index + 1, vf_instance, queue_index);
  }
}

static inline int tap_alive(void)
{
  return tap_fd != -1;
}

static void tap_destroy(int killed)
{
  signal(SIGINT, SIG_DFL);
  if (!killed)
    pthread_mutex_lock(&tap_mutex);
  if (tap_alive()) {
    struct ifreq ifr;

    init_ifreq(&ifr);
    close(tap_fd); tap_fd = -1;
    close(ctl_fd); ctl_fd = -1;
    fprintf(stderr, "shutdown interface %s\n", ifr.ifr_name);
    fprintf(stderr, "t2h: read %u, posted %u, completed %u\n",
            send_read, send_post, send_comp);
    fprintf(stderr, "h2t: posted %u, completed %u, wrote %u\n",
            recv_post, recv_comp, recv_write);
  }
  if (!killed)
    pthread_mutex_unlock(&tap_mutex);
}

void interrupt(int signal)
{
  tap_destroy(1);
}

static void tap_setup(void)
{
  struct ifreq ifr;
  struct sockaddr_in sin;
  int result;

  tap_fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
  if (tap_fd < 0) {
    die("Failed attempt to open /dev/net/tun.");
  }

  init_ifreq(&ifr);
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI ;
  result = ioctl(tap_fd, TUNSETIFF, (void *)&ifr);
  if (result < 0) {
    die("Failed attempt to configure tap device.");
  }

  ctl_fd = socket(AF_INET, SOCK_DGRAM, 0);

  if (if_addr) {
    init_ifreq(&ifr);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    if (inet_aton(if_addr, &sin.sin_addr) == 0) {
      die("Failed to parse interface address.");
    }
    memcpy(&ifr.ifr_addr, &sin, sizeof(ifr.ifr_addr));
    result = ioctl(ctl_fd, SIOCSIFADDR, (char *)&ifr); 
    if (result < 0) {
      die("Failed to configure interface address [%s].",
                   strerror(errno));
    }
  }

  init_ifreq(&ifr);
  result = ioctl(ctl_fd, SIOCGIFFLAGS, &ifr);
  if (result < 0) {
    die("Failed to get interface flags.");
  }
  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  result = ioctl(ctl_fd, SIOCSIFFLAGS, &ifr);
  if (result < 0) {
    die("Failed to get interface flags.");
  }

  signal(SIGINT, interrupt);
  fprintf(stderr, "initialized interface %s\n", ifr.ifr_name);
}

static inline tile_hdr_t* buf_header(void* buf_mem, unsigned index)
{
  return buf_mem + ((index & (MAX_PKTS - 1)) * MAX_PKT_SIZE);
}

static void do_send(gxpci_context_t* context, void* buf_mem)
{
  gxpci_comp_t comp;
  gxpci_cmd_t cmd;
  tile_hdr_t *hdr;
  int result;

  result = gxpci_iomem_register(context, buf_mem, MAP_LENGTH);
  if (result) {
    die("failed to register iomem.");
  }

  tmc_spin_barrier_wait(&barrier);

  send_read = 0;
  send_post = 0;
  send_comp = 0;

  while (tap_alive())
  {
    // read packets from the tap device
    while (send_read - send_comp < MAX_PKTS) {
      hdr = buf_header(buf_mem, send_read);
      result = read(tap_fd, hdr + 1, MAX_PKT_SIZE);
      if ((result == 0) || (result < 0 && errno == EAGAIN)) {
	break;
      } else if (result < 0) {
        fprintf(stderr, "tap read error, return %d, errno %s.\n",
                result, strerror(errno));
        return;
      } else if (result < MIN_PKT_SIZE) {
        fprintf(stderr, "runt t2h packet, size %d, index %u\n",
                result, send_read);
	continue;
      }

      hdr->size = result;

      debug("read index %d, size %d\n", send_read, hdr->size);
      send_read++;
    }

    __insn_mf();        // make sure all packet data is written out

    // post PQ commands
    while (send_read - send_post > 0) {
      result = gxpci_get_cmd_credits(context);
      if (result < 0) {
        fprintf(stderr, "t2h credit error.\n");
        return;
      } else if (result == 0) {
        break;
      }

      hdr = buf_header(buf_mem, send_post);
      cmd.buffer = hdr;
      cmd.size   = hdr->size + sizeof(*hdr);
      result = gxpci_pq_t2h_cmd(context, &cmd);
      if (result < 0) {
        fprintf(stderr, "t2h command error.\n");
        return;
      }

      debug("posted index %d, size %d\n", send_post, hdr->size);
      send_post++;
    }

    // reap PQ completions
    while (send_comp - send_post > 0) {
      result = gxpci_get_comps(context, &comp, 0, 1);
      if (result < 0) {
        fprintf(stderr, "t2h completion error.\n");
        return;
      } else if (result == 0) {
        break;
      }

      debug("completed index %d, size %d\n", send_comp, comp.size);
      send_comp++;
    }
  }
}

static void do_recv(gxpci_context_t* context, void* buf_mem)
{
  gxpci_comp_t comp;
  gxpci_cmd_t cmd;
  tile_hdr_t *hdr;
  int result;

  result = gxpci_iomem_register(context, buf_mem, MAP_LENGTH);
  if (result) {
    die("failed to register iomem.");
  }

  tmc_spin_barrier_wait(&barrier);

  recv_write = 0;
  recv_post  = 0;
  recv_comp  = 0;

  while (tap_alive())
  {
    // post empty buffers
    while (recv_post - recv_write < MAX_PKTS) {
      result = gxpci_get_cmd_credits(context);
      if (result < 0) {
        fprintf(stderr, "h2t credit error.\n");
        return;
      } else if (result == 0) {
        break;
      }

      hdr = buf_header(buf_mem, recv_post);
      cmd.buffer = hdr;
      cmd.size   = MAX_PKT_SIZE;
      result = gxpci_pq_h2t_cmd(context, &cmd);
      if (result < 0)
      {
        fprintf(stderr, "h2t command error.\n");
        return;
      }

      recv_post++;
    }

    // reap comp buffers
    while (recv_comp - recv_post > 0) {
      result = gxpci_get_comps(context, &comp, 0, 1);
      if (result < 0) {
        fprintf(stderr, "h2t completion error.\n");
        return;
      } else if (result == 0) {
        break;
      }

      recv_comp++;
    }

    // write out buffers over the tap device
    while (recv_comp - recv_write > 0) {
      hdr = buf_header(buf_mem, recv_write);

      if (hdr->size < MIN_PKT_SIZE) {
        fprintf(stderr, "runt h2t packet, size %d, index %u\n",
                hdr->size, recv_write);
	recv_write++;
	continue;
      }

      result = write(tap_fd, hdr + 1, hdr->size);
      if ((result == 0) || (result < 0 && errno == EAGAIN)) {
        break;
      } else if (result < 0) {
        fprintf(stderr, "tap write error, return %d, errno %s.\n",
                result, strerror(errno));
        return;
      }

      recv_write++;
    }
  }
}

static void* thread_func(void *arg)
{
  int rank = (intptr_t) arg;
  int cpu;

  if (rank == 0)
    cpu = tmc_cpus_find_nth_cpu(&desired_cpus, cpu_rank);
  else
    cpu = tmc_cpus_find_nth_cpu(&desired_cpus, cpu_rank + 1);

  if (tmc_cpus_set_my_cpu(cpu) < 0)
    die("Failure in 'tmc_cpus_set_my_cpu()'.");

  // Allocate and register data buffers.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
  assert(buf_mem);

  if (rank == 0)
    do_recv(h2t_context, buf_mem);
  else
    do_send(t2h_context, buf_mem);

  __insn_mf();

  tap_destroy(0);

  return (void*) NULL;
}

int main(int argc, char**argv)
{
  gxio_trio_context_t trio_context_body;
  gxio_trio_context_t* trio_context = &trio_context_body;

  // This indicates that we need to allocate an ASID ourselves,
  // instead of using one that is allocated somewhere else.
  int asid = GXIO_ASID_NULL;

  parse_args(argc, argv);

  // We must bind to a single CPU.
  if (tmc_cpus_get_my_affinity(&desired_cpus) != 0)
    die("tmc_cpus_get_my_affinity() failed.");

  // Bind to the current Tile in the cpu set.
  int cpu = tmc_cpus_get_my_current_cpu();
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, cpu)) < 0)
    die("tmc_cpus_set_my_cpu() failed.");

  do {
    tap_setup();

    // Get a gxio context.
    int result = gxio_trio_init(trio_context, trio_index);
    if (result) {
      die("Failed to initialize trio context.");
    }

    result = gxpci_init(trio_context, h2t_context, trio_index, mac_index);
    if (result) {
      die("Failed to initialize h2t context.");
    }

    result = gxpci_open_queue(h2t_context, asid, h2t_type, vf_instance,
                              queue_index, 0, 0);
    if (result) {
      die("Failed to open h2t queue.");
    }

    result = gxpci_init(trio_context, t2h_context, trio_index, mac_index);
    if (result) {
      die("Failed to initialize t2h context.");
    }

    result = gxpci_open_queue(t2h_context, asid, t2h_type, vf_instance,
                              queue_index, 0, 0);
    if (result) {
      die("Failed to open h2t queue.");
    }

    // Barrier init.
    tmc_spin_barrier_init(&barrier, NUM_THREADS);

    __insn_mf();

    // Go parallel.
    for (int i = 0; i < NUM_THREADS; i++)
    {
      if (pthread_create(&threads[i], NULL, thread_func, (void*)(intptr_t)i) != 0)
        die("pthread_create() failed.");
    }

    // Wait for all threads.
    for (int i = 0; i < NUM_THREADS; i++)
    {
      if (pthread_join(threads[i], NULL) != 0)
        die("pthread_join() failed.");
    }

    gxpci_destroy(h2t_context);
    gxpci_destroy(t2h_context);
  } while (restart);

  return 0;
}
