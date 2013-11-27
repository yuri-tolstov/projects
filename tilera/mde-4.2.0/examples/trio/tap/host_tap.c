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

#include <asm/tilegxpci.h>

#define die(fmt, args...)                       \
  do {                                          \
    fprintf(stderr, fmt "\n" , ##args);         \
    exit(EXIT_FAILURE);                         \
  } while (0)

#define debug(...)

typedef struct {
  char                                  dev_name[40];
  struct tlr_pq_status*                 status;
  struct gxpci_host_pq_regs_app*        regs;
  void*                                 buf_mem;
  int                                   pq_fd;
  unsigned                              producer_index;
  unsigned                              consumer_index;
} host_pq_t;

#define NUM_THREADS		2
#define MIN_PKT_SIZE		32
#define MAX_PKT_SIZE		(4 * 1024)
#define MAX_PKTS                GXPCI_HOST_PQ_SEGMENT_ENTRIES
#define MAP_LENGTH 		(MAX_PKTS * MAX_PKT_SIZE)

static int queue_index = 0;     // Queue index for both sender and receiver.
static int card_index = 0;      // GX card index.
static int tap_fd = -1;         // TAP device descriptor.
static int ctl_fd;              // Control FD for interface operations.
static int restart = 1;         // Phoenix mode - restart on death.


static pthread_t threads[NUM_THREADS];

static const char *if_name = NULL;
static const char *if_addr = NULL;

static pthread_mutex_t tap_mutex = PTHREAD_MUTEX_INITIALIZER;

static host_pq_t h2t, t2h;

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

    if ((opt = shift_option(&args, "--if-name=")))
      if_name = opt;
    else if ((opt = shift_option(&args, "--if-addr=")))
      if_addr = opt;
    else if ((opt = shift_option(&args, "--queue=")))
      queue_index = atoi(opt);
    else if ((opt = shift_option(&args, "--card=")))
      card_index = atoi(opt);
    else if ((opt = shift_option(&args, "--no-restart")))
      restart = 0;
    else
    {
      die("unknown option '%s'.", args[0]);
    }
  }
}

static void init_ifreq(struct ifreq *ifr)
{
  memset(ifr, 0, sizeof(ifr));
  if (if_name) {
    strncpy(ifr->ifr_name, if_name, sizeof(ifr->ifr_name));
  } else {
    snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "gxpcipq%d-%d",
             card_index + 1, queue_index);
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
    fprintf(stderr, "t2h (loc): producer %u, consumer %u\n",
            t2h.producer_index, t2h.consumer_index);
    fprintf(stderr, "t2h (reg): producer %u, consumer %u\n",
            t2h.regs->producer_index, t2h.regs->consumer_index);
    fprintf(stderr, "h2t (loc): producer %u, consumer %u\n",
            h2t.producer_index, h2t.consumer_index);
    fprintf(stderr, "h2t (reg): producer %u, consumer %u\n",
            h2t.regs->producer_index, h2t.regs->consumer_index);
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
    die("Failed attempt to configure tap device, %s.", strerror(errno));
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

static void pq_setup(host_pq_t* pq)
{
  // Open the packet queue file.
  pq->pq_fd = open(pq->dev_name, O_RDWR);
  if (pq->pq_fd < 0) {
    die("Failed to open PQ device.");
  }

  // Map the register space.
  pq->regs = mmap(0, sizeof(*pq->regs), PROT_READ | PROT_WRITE, MAP_SHARED,
                  pq->pq_fd, TILEPCI_PACKET_QUEUE_INDICES_MMAP_OFFSET);
  if (pq->regs == MAP_FAILED) {
    die("Failed to map PQ registers.");
  }

  // Configure and allocate the DMA ring buffer.
  tilepci_packet_queue_info_t buf_info = { .buf_size = MAX_PKT_SIZE };

  int error = ioctl(pq->pq_fd, TILEPCI_IOC_SET_PACKET_QUEUE_BUF, &buf_info);
  if (error < 0) {
    die("Failed to configure PQ buffers.");
  }

  // Map the DMA ring
  pq->buf_mem = mmap(0, MAP_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED,
                    pq->pq_fd, TILEPCI_PACKET_QUEUE_BUF_MMAP_OFFSET);
  if (pq->buf_mem == MAP_FAILED) {
    die("Failed to map PQ ring.");
  }

  // Map the status registers
  pq->status = mmap(0, sizeof(*pq->status), PROT_READ | PROT_WRITE, MAP_SHARED,
                    pq->pq_fd, TILEPCI_PACKET_QUEUE_STS_MMAP_OFFSET);
  if (pq->status == MAP_FAILED) {
    die("Failed to map PQ status.");
  }
}

static void pq_destroy(host_pq_t *pq)
{
  munmap(pq->status, sizeof(*pq->status));
  munmap(pq->buf_mem, MAP_LENGTH);
  munmap(pq->regs, sizeof(*pq->regs));
  close(pq->pq_fd);
}

static inline unsigned wrap_index(unsigned index)
{
  return index & (MAX_PKTS - 1);
}

static inline tile_hdr_t* buf_header(host_pq_t* pq, unsigned index)
{
  return pq->buf_mem + ((index & (MAX_PKTS - 1)) * MAX_PKT_SIZE);
}

static void do_send(host_pq_t* pq)
{
  tile_hdr_t *hdr;
  int result;

  while (tap_alive())
  {
    // check remote status
    if (pq->status->status == GXPCI_CHAN_RESET) {
      return;
    }

    // sync local and remote indices
    pq->regs->producer_index = pq->producer_index;
    pq->consumer_index = pq->regs->consumer_index;

    // read more packets if we have room
    while (pq->producer_index - pq->consumer_index < MAX_PKTS) {
      hdr = buf_header(pq, pq->producer_index);
      result = read(tap_fd, hdr + 1, MAX_PKT_SIZE);
      if ((result == 0) || (result < 0 && errno == EAGAIN)) {
        break;
      } else if (result < 0) {
        fprintf(stderr, "tap read error, return %d, errno %s.\n",
                result, strerror(errno));
        return;
      } else if (result < MIN_PKT_SIZE) {
        fprintf(stderr, "h2t runt packet, size %d, index %u\n",
                result, pq->producer_index);
        continue;
      }

      hdr->size = result;
      pq->producer_index ++;
    }
  }
}

static void do_recv(host_pq_t* pq)
{
  tile_hdr_t *hdr;
  int result;

  while (tap_alive())
  {
    // check remote status
    if (pq->status->status == GXPCI_CHAN_RESET) {
      return;
    }

    // sync local and remote indices
    pq->regs->consumer_index = pq->consumer_index;
    pq->producer_index = pq->regs->producer_index;

    // write out packets if we have any
    while (pq->producer_index - pq->consumer_index) {
      hdr = buf_header(pq, pq->consumer_index);
      debug("posting index %d, size %d\n", pq->consumer_index, hdr->size);

      if (hdr->size < MIN_PKT_SIZE) {
        fprintf(stderr, "t2h runt packet, size %d, index %u\n", result,
                pq->consumer_index);
        pq->consumer_index++;
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

      pq->consumer_index++;
    }
  }
}

static void* thread_func(void *arg)
{
  int rank = (intptr_t) arg;

  if (rank == 0)
    do_recv(&t2h);
  else
    do_send(&h2t);

  tap_destroy(0);

  return (void*) NULL;
}

int main(int argc, char**argv)
{

  parse_args(argc, argv);

  do {
    tap_setup();

    memset(&h2t, 0, sizeof(h2t));
    snprintf(h2t.dev_name, sizeof(h2t.dev_name),
             "/dev/tilegxpci%d/packet_queue/h2t/%d", card_index, queue_index);
    pq_setup(&h2t);
    h2t.regs->producer_index = 0;

    memset(&t2h, 0, sizeof(t2h));
    snprintf(t2h.dev_name, sizeof(t2h.dev_name),
             "/dev/tilegxpci%d/packet_queue/t2h/%d", card_index, queue_index);
    pq_setup(&t2h);
    t2h.regs->consumer_index = 0;

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

    pq_destroy(&h2t);
    pq_destroy(&t2h);
  } while (restart);

  return 0;
}
