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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gxio/trio.h>
#include <gxpci/gxpci.h>
#include <tmc/cpus.h>
#include <tmc/alloc.h>
#include <tmc/task.h>

#include <arch/cycle.h>

#include <tmc/perf.h>

#define DATA_VALIDATION

#if 0
#define PRINT_CMD_INFO
#endif

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxpci_strerror(__val));       \
  } while (0)

// The packet pool memory size.
#define MAP_LENGTH 		(16 * 1024 * 1024)

#define MAX_PKT_SIZE		(1 << (GXPCI_MAX_CMD_SIZE_BITS - 1))

#define PKTS_IN_POOL		(MAP_LENGTH / MAX_PKT_SIZE)

// The number of packets that this program sends.
#define SEND_PKT_COUNT 		300000

// The size of a single packet.
#define SEND_PKT_SIZE           (4096)

// The size of receive buffers posted by the receiver.
#define RECV_BUFFER_SIZE        (4096)

// The size of space that the receiver wants to preserve
// at the beginning of the packet, e.g. for packet header
// that is to be filled after the packet is received.
#if 0
#define RECV_PKT_HEADROOM	14
#else
#define RECV_PKT_HEADROOM	0	
#endif

//
// These are the spaces leading and trailing the packet
// that are inspected to validate DMA correctness.
//
#if 0
#define PKT_CLEAR_SPACE         16
#else
#define PKT_CLEAR_SPACE         0
#endif

cpu_set_t desired_cpus;

// The running cpu number.
int cpu_rank = 1;

// The TRIO index.
int trio_index = 0;

// The queue index of a C2C queue, used by both sender and receiver.
int queue_index;

int rem_link_index;

// The local MAC index.
int loc_mac;

int send_pkt_count = SEND_PKT_COUNT;

int send_pkt_size = SEND_PKT_SIZE;

static void usage(void)
{
  fprintf(stderr, "Usage: c2c_receiver [--mac=<local mac port #>] "
          "[--rem_link_index=<remote port link index>] "
          "[--queue_index=<send queue index>] "
          "[--send_pkt_size=<packet size>] "
          "[--cpu_rank=<cpu_id>] "
          "[--send_pkt_count=<packet count>]\n");
  exit(EXIT_FAILURE);
}

static char *
shift_option(char ***arglist, const char* option)
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
void parse_args(int argc, char** argv)
{
  char **args = &argv[1];

  // Scan options.
  //
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--mac=")))
      loc_mac = atoi(opt);
    else if ((opt = shift_option(&args, "--rem_link_index=")))
      rem_link_index = atoi(opt);
    else if ((opt = shift_option(&args, "--queue_index=")))
      queue_index = atoi(opt);
    else if ((opt = shift_option(&args, "--send_pkt_size=")))
      send_pkt_size = atoi(opt);
    else if ((opt = shift_option(&args, "--cpu_rank=")))
      cpu_rank = atoi(opt);
    else if ((opt = shift_option(&args, "--send_pkt_count=")))
      send_pkt_count = atoi(opt);
    else if ((opt = shift_option(&args, "--cpu_rank=")))
      cpu_rank = atoi(opt);
    else
      usage();
  }
}

void do_recv(gxpci_context_t* context, void* buf_mem)
{
  uint64_t finish_cycles;
  float gigabits;
  float cycles;
  float gbps;
  uint64_t start_cycles = 0;
  unsigned int sent_pkts = 0;
  int result;
  int recv_pkt_count = 0;

  gxpci_cmd_t cmd = {
    .buffer = buf_mem,
    .size = RECV_BUFFER_SIZE,
  };

#ifdef DATA_VALIDATION
  int recv_pkt_size;

  if (RECV_PKT_HEADROOM + send_pkt_size > RECV_BUFFER_SIZE)
    recv_pkt_size = RECV_BUFFER_SIZE - RECV_PKT_HEADROOM;
  else
    recv_pkt_size = send_pkt_size;

  //
  // Receive one packet first to validate the DMA correctness.
  //
  char* source = (char*)buf_mem;
  for (int i = 0; i < (PKT_CLEAR_SPACE + RECV_PKT_HEADROOM); i++)
    source[i] = 0x55;

  source += PKT_CLEAR_SPACE + RECV_PKT_HEADROOM + recv_pkt_size;
  for (int i = 0; i < PKT_CLEAR_SPACE; i++)
    source[i] = 0x55;

  cmd.buffer = buf_mem + PKT_CLEAR_SPACE;
  result = gxpci_post_cmd(context, &cmd);
  VERIFY_ZERO(result, "gxpci_post_cmd()");

  sent_pkts++;

  gxpci_comp_t comp;
  result = gxpci_get_comps(context, &comp, 1, 1);
  if (result == GXPCI_ERESET)
  {
    printf("do_recv: channel is reset\n");
    goto recv_reset;
  }

  if (recv_pkt_size != comp.size)
    tmc_task_die("Validation packet size error, expecting %d, getting %d.\n",
                 recv_pkt_size, comp.size);

  // Check results.
  source = (char*)buf_mem;

  for (int i = 0; i < (PKT_CLEAR_SPACE + RECV_PKT_HEADROOM); i++)
  {
    if (source[i] != 0x55)
      tmc_task_die("Leading data corruption at byte %d, %d.\n", i, source[i]);
  }

  source += PKT_CLEAR_SPACE + RECV_PKT_HEADROOM;
  for (int i = 0; i < recv_pkt_size; i++)
  {
    if (source[i] != (char)(i + (i >> 8) + 1))
      tmc_task_die("Data payload corruption at byte %d, %d.\n", i, source[i]);
  }

  source += recv_pkt_size;
  for (int i = 0; i < PKT_CLEAR_SPACE; i++)
  {
    if (source[i] != 0x55)
      tmc_task_die("Trailing data corruption at byte %d, addr %p value %d.\n",
                   i, &source[i], source[i]);
  }

  printf("Data validation test passed.\n");

#endif

  start_cycles = get_cycle_count();

  while (recv_pkt_count < send_pkt_count)
  {
    int cmds_to_post;
    int credits;

    credits = gxpci_get_cmd_credits(context);
#ifdef DATA_VALIDATION
    cmds_to_post = MIN(credits, (send_pkt_count + 1 - sent_pkts));
#else
    cmds_to_post = MIN(credits, send_pkt_count - sent_pkts));
#endif
    for (int i = 0; i < cmds_to_post; i++)
    {
      cmd.buffer = buf_mem +
        ((sent_pkts + i) % PKTS_IN_POOL) * MAX_PKT_SIZE;

      result = gxpci_post_cmd(context, &cmd);
      if (result == GXPCI_ERESET)
      {
        printf("do_recv: channel is reset\n");
        goto recv_reset;
      }
      else if (result == GXPCI_ECREDITS)
        break;

      VERIFY_ZERO(result, "gxpci_post_cmd()");
      sent_pkts++;
    }

    gxpci_comp_t comp[64];

    result = gxpci_get_comps(context, comp, 0, 64);
    if (result == GXPCI_ERESET)
    {
      printf("do_recv: channel is reset\n");
      goto recv_reset;
    }

    for (int i = 0; i < result; i++)
    {
#ifdef PRINT_CMD_INFO
      printf("Recv buf addr: %#lx size: %d\n",
        (unsigned long)comp[i].buffer, comp[i].size);
#endif
      recv_pkt_count++;
    }
  }

recv_reset:

  finish_cycles = get_cycle_count();
  gigabits = (float)recv_pkt_count * send_pkt_size * 8;
  cycles = finish_cycles - start_cycles;
  gbps = gigabits / cycles * tmc_perf_get_cpu_speed() / 1e9;
  printf("Received %d %d-byte packets: %f gbps\n",
         recv_pkt_count, send_pkt_size, gbps);

  gxpci_destroy(context);
}

int main(int argc, char**argv)
{
  gxio_trio_context_t trio_context_body;
  gxio_trio_context_t* trio_context = &trio_context_body;
  gxpci_context_t gxpci_context_body;
  gxpci_context_t* gxpci_context = &gxpci_context_body;

  parse_args(argc, argv);

  assert(send_pkt_size <= GXPCI_MAX_CMD_SIZE);

  //
  // We must bind to a single CPU.
  //
  if (tmc_cpus_get_my_affinity(&desired_cpus) != 0)
     tmc_task_die("tmc_cpus_get_my_affinity() failed.");

  // Bind to the cpu_rank'th tile in the cpu set
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, cpu_rank)) < 0)
     tmc_task_die("tmc_cpus_set_my_cpu() failed.");

  //
  // This indicates that we need to allocate an ASID ourselves,
  // instead of using one that is allocated somewhere else.
  //
  int asid = GXIO_ASID_NULL;

  //
  // Get a gxio context.
  //
  int result = gxio_trio_init(trio_context, trio_index);
  VERIFY_ZERO(result, "gxio_trio_init()");

  result = gxpci_init(trio_context, gxpci_context, trio_index, loc_mac);
  VERIFY_ZERO(result, "gxpci_init()");

  result = gxpci_open_queue(gxpci_context, asid,
                            GXPCI_C2C_RECV,
                            rem_link_index,
                            queue_index,
                            RECV_PKT_HEADROOM,
                            RECV_BUFFER_SIZE);
  if (result == GXPCI_ERESET)
  {
    gxpci_destroy(gxpci_context);
    exit(EXIT_FAILURE);
  }
  VERIFY_ZERO(result, "gxpci_open_queue()");

  //
  // Allocate and register data buffers.
  //
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
  assert(buf_mem);

  result = gxpci_iomem_register(gxpci_context, buf_mem, MAP_LENGTH);
  VERIFY_ZERO(result, "gxpci_iomem_register()");

  printf("c2c_receiver running on cpu %d, mac %d rem_link_index %d queue %d\n",
         cpu_rank, loc_mac, rem_link_index, queue_index);

  // Run the test.
  do_recv(gxpci_context, buf_mem);

  return 0;
}
