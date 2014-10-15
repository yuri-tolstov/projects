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
// Test program to validate chip-to-chip data transfers and measure
// data transfer throughput between two TILE-Gx PCIe ports.
//
// This program spawns one sender thread and one receiver thread,
// exchanging data packets with a pair of send and receiver threads
// running on a remote PCIe port, using two unidirectional queues.
//
// The program is typically run with the following parameters on one node:
//   c2c --rem_link_index=X --send_queue_index=M --recv_queue_index=N
// which runs a sender thread using queue index M and a receiver thread using
// queue index N, talking to a remote port with link index X.
//
// On another node,
//   c2c --rem_link_index=Y --send_queue_index=N --recv_queue_index=M
// which runs a sender thread using queue index N and a receiver thread using
// queue index M, talking to a remote port with link index Y.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <gxio/trio.h>
#include <gxpci/gxpci.h>
#include <tmc/cpus.h>
#include <tmc/alloc.h>
#include <tmc/task.h>
#include <tmc/sync.h>
#include <tmc/perf.h>

#include <arch/cycle.h>

#if 0
#define TEST_DATA_PATTERN
#endif

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxpci_strerror(__val));       \
  } while (0)

// The packet pool memory size.
#define MAP_LENGTH 		tmc_alloc_get_huge_pagesize()

// The number of packets that this program sends.
#define SEND_PKT_COUNT		300000

// The size of a single packet.
#define SEND_PKT_SIZE		4096

#define MAX_CMDS_BATCH		64

gxio_trio_context_t trio_context_body;
gxio_trio_context_t *trio_context = &trio_context_body;

gxpci_context_t gxpci_context_send;
gxpci_context_t *send_context = &gxpci_context_send;
gxpci_context_t gxpci_context_recv;
gxpci_context_t *recv_context = &gxpci_context_recv;

cpu_set_t desired_cpus;

// By default, this test conducts duplex transfers.
int dont_recv = 0;
int dont_send = 0;

// The running cpu number.
int send_cpu_rank = 1;
int recv_cpu_rank = 2;

// The TRIO index.
int trio_index = 0;
int trio_asid;

int rem_link_index = 1;

// The local MAC index.
int loc_mac = 0;

// The queue index of the C2C send queue and receive queue.
int send_queue_index = 1;
int recv_queue_index = 2;

int send_pkt_count = SEND_PKT_COUNT;

int send_pkt_size = SEND_PKT_SIZE;

int pkt_size_order;

// The size of space that the receiver wants to preserve
// at the beginning of the packet, e.g. for packet header
// that is to be filled after the packet is received.
#if 0
// Note that no attempt was made to make non-zero RECV_PKT_HEADROOM
// work with TEST_DATA_PATTERN in this test.
#define RECV_PKT_HEADROOM       14
#else
#define RECV_PKT_HEADROOM       0
#endif

static void usage(void)
{
  fprintf(stderr, "Usage: c2c [--mac=<local mac port #>] "
          "[--dont_recv=<1>] "
          "[--dont_send=<1>] "
          "[--rem_link_index=<remote port link index>] "
          "[--send_queue_index=<send queue index>] "
          "[--recv_queue_index=<receive queue index>] "
          "[--send_pkt_size=<packet size>] "
          "[--send_cpu_rank=<cpu_id>] "
          "[--recv_cpu_rank=<cpu_id>] "
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
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--mac=")))
      loc_mac = atoi(opt);
    else if ((opt = shift_option(&args, "--dont_recv=")))
      dont_recv = atoi(opt);
    else if ((opt = shift_option(&args, "--dont_send=")))
      dont_send= atoi(opt);
    else if ((opt = shift_option(&args, "--rem_link_index=")))
      rem_link_index = atoi(opt);
    else if ((opt = shift_option(&args, "--send_queue_index=")))
      send_queue_index = atoi(opt);
    else if ((opt = shift_option(&args, "--recv_queue_index=")))
      recv_queue_index = atoi(opt);
    else if ((opt = shift_option(&args, "--send_pkt_size=")))
      send_pkt_size = atoi(opt);
    else if ((opt = shift_option(&args, "--send_cpu_rank=")))
      send_cpu_rank = atoi(opt);
    else if ((opt = shift_option(&args, "--recv_cpu_rank=")))
      recv_cpu_rank = atoi(opt);
    else if ((opt = shift_option(&args, "--send_pkt_count=")))
      send_pkt_count = atoi(opt);
    else
      usage();
  }
}

static unsigned int next_power_of_2(int pkt_size)
{
  if (pkt_size & (pkt_size - 1))
    return 32 - __builtin_clz(pkt_size);
  else
    return __builtin_ctz(pkt_size);
}

void do_send(gxpci_context_t* context, void* buf_mem)
{
  gxpci_comp_t comp[MAX_CMDS_BATCH];
  gxpci_cmd_t cmd[MAX_CMDS_BATCH];
  uint64_t start_cycles;
  uint64_t finish_cycles;
  float gigabits;
  float cycles;
  float gbps;
  int result;
  uint32_t cmds_to_post;
  uint32_t completions = 0;
  uint32_t sends = 0;
  uint32_t credits;
#ifdef TEST_DATA_PATTERN
  uint32_t data_pattern = 0;
  int j;
#endif

  start_cycles = get_cycle_count();

  while (completions < send_pkt_count)
  {
    if (sends < send_pkt_count)
    {
      credits = gxpci_c2c_send_get_credits(context);
      cmds_to_post = MIN(credits, (send_pkt_count - sends));
      cmds_to_post = MIN(MAX_CMDS_BATCH, cmds_to_post);

      for (int i = 0; i < cmds_to_post; i++)
      {
        cmd[i].buffer =
          buf_mem + ((sends++ << pkt_size_order) & (MAP_LENGTH - 1));
        cmd[i].size = send_pkt_size;

#ifdef TEST_DATA_PATTERN
        for (j = 0; j < send_pkt_size; j += 4)
        {
          *((uint32_t*)(cmd[i].buffer + j)) = data_pattern++;
        }
        // Make data visible to push DMA command.
        __insn_mf();
#endif

      }

      result = gxpci_c2c_send_cmds(context, cmd, cmds_to_post);
      if (result == GXPCI_ERESET)
      {
        printf("do_send: channel is reset, posted %u comps %u\n",
               sends - cmds_to_post, completions);
        goto send_reset;
      }
    }

    result = gxpci_c2c_get_send_comps(context, comp, 0, MAX_CMDS_BATCH);
    if (result == GXPCI_ERESET)
    {
      printf("do_send: channel is reset, posted %u comps %u\n",
             sends, completions);
      goto send_reset;
    }
    completions += result;
  }

send_reset:

  finish_cycles = get_cycle_count();
  gigabits = (float)completions * send_pkt_size * 8;
  cycles = finish_cycles - start_cycles;
  gbps = gigabits / cycles * tmc_perf_get_cpu_speed() / 1e9;
  printf("Transferred %d %d-byte packets: %f gbps\n",
         completions, send_pkt_size, gbps);

  gxpci_c2c_send_destroy(context);
}

void *thread_send(void* arg)
{
  gxpci_context_t *gxpci_context = (gxpci_context_t *)arg;
  int rank = send_cpu_rank;
  int result;

  // Bind to the rank'th tile in the cpu set.
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, rank)) < 0)
    tmc_task_die("tmc_cpus_set_my_cpu(thread_send) failed.");
 
  if (dont_send)
    return (void*)NULL;	

  // Allocate and register data buffers.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
  assert(buf_mem);

  result = gxio_trio_register_page(trio_context, trio_asid, buf_mem,
                                   MAP_LENGTH, 0);
  VERIFY_ZERO(result, "gxio_trio_register_page()");

  result = gxpci_c2c_open_send_queue(gxpci_context, send_queue_index);
  VERIFY_ZERO(result, "gxpci_c2c_open_send_queue()");

  printf("thread_send(tile %d): queue %d opened, waiting to send ...\n",
         rank, send_queue_index);

  // Run the test.
  do_send(gxpci_context, buf_mem);

  return (void*)NULL;
}

void do_recv(gxpci_context_t* context, void* buf_mem)
{
  gxpci_comp_t comp[MAX_CMDS_BATCH];
  gxpci_cmd_t cmd[MAX_CMDS_BATCH];
  uint32_t recv_pkt_count = 0;
  unsigned int sent_pkts = 0;
  unsigned int cmds_to_post;
  unsigned int credits;
  int result;
#ifdef TEST_DATA_PATTERN
  uint32_t actual_data;
  uint32_t expected_data = 0;
  uint32_t *chk_buf;
#endif

  while (recv_pkt_count < send_pkt_count)
  {
    credits = gxpci_c2c_recv_get_credits(context);
    cmds_to_post = MIN(credits, send_pkt_count - sent_pkts);
    cmds_to_post = MIN(cmds_to_post, MAX_CMDS_BATCH);

    for (int i = 0; i < cmds_to_post; i++)
    {
      cmd[i].buffer =
        buf_mem + ((sent_pkts++ << pkt_size_order) & (MAP_LENGTH - 1));
      cmd[i].size = send_pkt_size;
    }

    result = gxpci_c2c_recv_cmds(context, cmd, cmds_to_post);
    if (result == GXPCI_ERESET)
    {
      printf("do_recv: channel is reset, posted %u received %u\n",
             sent_pkts - cmds_to_post, recv_pkt_count);
      goto recv_reset;
    }

    result = gxpci_c2c_get_recv_comps(context, comp, 0, MAX_CMDS_BATCH);
    if (result == GXPCI_ERESET)
    {
      printf("do_recv: channel is reset, posted %u received %u\n",
             sent_pkts, recv_pkt_count);
      goto recv_reset;
    }

#ifdef TEST_DATA_PATTERN
    for (int i = 0; i < result; i++)
    {
      chk_buf = (uint32_t *)(comp[i].buffer);
      for (int j = 0; j < (send_pkt_size >> 2); j++)
      {
        actual_data = chk_buf[j];
        if (actual_data != expected_data)
        {
          printf("do_recv: comp %d of %d word %d expect %d get %d %p "
                 "posted %u recved %u\n", i, result, j, expected_data,
                 actual_data, chk_buf + j, sent_pkts, recv_pkt_count);

          goto recv_reset;
        }
        expected_data++;
      }
      recv_pkt_count++;
    }
#else
    recv_pkt_count += result;
#endif
  }

recv_reset:

  printf("Received %d %d-byte packets\n", recv_pkt_count, send_pkt_size);

  gxpci_c2c_recv_destroy(context);
}

void *thread_recv(void *arg)
{
  gxpci_context_t *gxpci_context = (gxpci_context_t *)arg;
  int rank = recv_cpu_rank;
  int result;
 
  // Bind to the rank'th tile in the cpu set (RECV).
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, rank)) < 0)
    tmc_task_die("tmc_cpus_set_my_cpu() failed.");
 
  if (dont_recv)
    return (void*)NULL;	

  // Allocate and register data buffers.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
  assert(buf_mem);

#ifdef TEST_DATA_PATTERN
  for (int i = 0; i < MAP_LENGTH;)
  {
    *(uint32_t *)(buf_mem + i) = 11;
    i += 4;
  }
#endif

  result = gxio_trio_register_page(trio_context, trio_asid, buf_mem,
                                   MAP_LENGTH, 0);
  VERIFY_ZERO(result, "gxio_trio_register_page()");
  
  result = gxpci_c2c_open_recv_queue(gxpci_context, RECV_PKT_HEADROOM,
                                     MAP_LENGTH, buf_mem, recv_queue_index);
  VERIFY_ZERO(result, "gxpci_c2c_open_recv_queue()");
  
  printf("thread_recv(tile %d): queue %d opened, waiting to receive ...\n",
         rank, recv_queue_index);
  
  // Run the test.
  do_recv(gxpci_context, buf_mem);

  return (void*)NULL;	
}

int main(int argc, char**argv)
{
  pthread_t send_thread;
  pthread_t recv_thread;
  int result;

  parse_args(argc, argv);
 
  if (tmc_cpus_get_my_affinity(&desired_cpus) != 0)
    tmc_task_die("tmc_cpus_get_my_affinity() failed.");
  
  // Initialize TRIO context and get a TRIO asid.
  // They are shared by the send and the receiv threads.
  result = gxio_trio_init(trio_context, trio_index);
  VERIFY_ZERO(result, "gxio_trio_init()");

  trio_asid = gxio_trio_alloc_asids(trio_context, 1, 0, 0);
  if (trio_asid < 0) {
	  tmc_task_die("Failure in gxio_trio_alloc_asids(), asid = %d",
		      trio_asid);
  }
 
  result = gxpci_init(trio_context, send_context, trio_index, loc_mac);
  VERIFY_ZERO(result, "gxpci_init(send_context)");

  result = gxpci_init(trio_context, recv_context, trio_index, loc_mac);
  VERIFY_ZERO(result, "gxpci_init(recv_context)");

  result = gxpci_open_duplex_queue(recv_context, send_context, trio_asid,
                                   GXPCI_C2C_DUPLEX, rem_link_index, 0);
  VERIFY_ZERO(result, "gxpci_open_duplex_queue");

  pkt_size_order = next_power_of_2(send_pkt_size);

  // Start pthreads to send and receive data.
  
  if (pthread_create(&recv_thread, NULL, thread_recv,
      (void*)recv_context) != 0)
    tmc_task_die("pthread_create(recv_thread) failed.");
  
  if (pthread_create(&send_thread, NULL, thread_send,
      (void*)send_context) != 0)
    tmc_task_die("pthread_create(send_thread) failed.");

  if (pthread_join(recv_thread, NULL) != 0)
    tmc_task_die("thread_join(recv_thread) failed.");
  
  if (pthread_join(send_thread, NULL) != 0)
    tmc_task_die("thread_join(send_thread) failed.");
  
  gxpci_destroy_duplex(recv_context, send_context, GXPCI_C2C_DUPLEX);

  return 0;
}
