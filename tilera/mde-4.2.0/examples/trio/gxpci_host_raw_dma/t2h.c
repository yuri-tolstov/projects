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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <gxio/trio.h>
#include <gxpci/gxpci.h>

#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/perf.h>
#include <tmc/sync.h>
#include <tmc/task.h>

#include <arch/cycle.h>

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxpci_strerror(__val));       \
  } while (0)

// we allocate 8 huge pages to serve as packet buffer,
#define T2H_HUGE_PAGES		8

// The packet memory pool.
void *t2h_page[T2H_HUGE_PAGES];

#define MAP_LENGTH 		(16 * 1024 * 1024)

#define MAX_PKT_SIZE		2048

#define TOTAL_BYTES_ORDER	28

#define MAX_CMDS_BATCH		64

// Number of queues that share the same huge page for pkt buffer.
// Since max number of H2T queues is 16, each queue uses half the huge page.
#define PAGE_SHARER		2

#define PKTS_IN_POOL		((MAP_LENGTH / PAGE_SHARER) / MAX_PKT_SIZE)

gxio_trio_context_t trio_context_body;
gxio_trio_context_t* trio_context = &trio_context_body;

gxpci_raw_dma_state_t rd_state_body;
gxpci_raw_dma_state_t *rd_state = &rd_state_body;

cpu_set_t desired_cpus;

// The TRIO index.
int trio_index = 0;
int trio_asid;

// The local MAC index.
int loc_mac = 0;

size_t packet_size = 64;
int num_senders;

// The host DMA buffer size.
unsigned int host_buf_size;

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
    else if ((opt = shift_option(&args, "--size=")))
      packet_size = atoi(opt);
    else
    {
      fprintf(stderr, "Unknown option '%s'.\n", args[0]);
      fprintf(stderr,
              "Usage: t2h --mac=0 --size=1344\n");
      exit(EXIT_FAILURE);
    }
  }
}

void do_send(gxpci_context_t* context, void* buf_mem)
{
  uint64_t start_cycles;
  uint64_t finish_cycles;
  unsigned int seq_num = 0;
  unsigned send_seq_num = 0;
  unsigned long cmds_total = (1ULL << TOTAL_BYTES_ORDER) / packet_size;
  int cpu;
  int result;
  int slots_in_host_buffer = host_buf_size / MAX_PKT_SIZE;
  gxpci_comp_t comp[MAX_CMDS_BATCH];
  gxpci_dma_cmd_t cmd[MAX_CMDS_BATCH];
  gxpci_dma_cmd_t template = {
    .buffer = NULL,
    .size = packet_size,
  };
 
  for (int i = 0; i < MAX_CMDS_BATCH; i++)
    cmd[i] = template;

  cpu = tmc_cpus_get_my_current_cpu();

  printf("do_send() started on tile %d, total = %lu\n", cpu, cmds_total);

  start_cycles = get_cycle_count();
  
  while (1)
  {
    if (seq_num < cmds_total)
    {
      unsigned credits = gxpci_raw_dma_send_get_credits(context);
      unsigned count = MIN(MAX_CMDS_BATCH, credits);

      count = MIN(count, (cmds_total - seq_num));

      // Post the next 'count' buffers.
      for (int i = 0; i < count; i++)
      {
        cmd[i].buffer = buf_mem +
          ((send_seq_num + i) % PKTS_IN_POOL) * MAX_PKT_SIZE;
        cmd[i].remote_buf_offset =
          ((send_seq_num + i) % slots_in_host_buffer) * MAX_PKT_SIZE;

        // Make data visible to TRIO.
        __insn_mf();

        result = gxpci_raw_dma_send_cmd(context, &cmd[i]);
        if (result == GXPCI_ERESET)
        {
          printf("do_send: channel is reset\n");
          goto send_reset;
        }
        VERIFY_ZERO(result, "gxpci_raw_dma_send_cmd()");
      }
      send_seq_num += count;
    }
    else
      break;

    int comp_count =
      gxpci_raw_dma_send_get_comps(context, comp, 0, MAX_CMDS_BATCH);
    if (comp_count == GXPCI_ERESET)
    {
      printf("do_send: channel is reset\n");
      goto send_reset;
    }

    seq_num += comp_count;
  }
  
send_reset:

  finish_cycles = get_cycle_count();

  // All done.  Print the performance results to stdout.
  float gigabits = (float)seq_num * packet_size * 8;
  float cycles = finish_cycles - start_cycles;
  float pps = (float)seq_num / cycles * tmc_perf_get_cpu_speed();
  float gbps = gigabits / cycles * tmc_perf_get_cpu_speed() / 1e9;
  printf("thread_send (%d): Transfered %u %d-byte packets: %f gbps, %f pps\n",
	 cpu, seq_num, (int)packet_size, gbps, pps);

  gxpci_raw_dma_send_destroy(context);
}

void *thread_send(void* arg)
{
  int result;
  int rank = (intptr_t) arg;
  int queue_index = rank;
  gxpci_context_t gxpci_context_body;
  gxpci_context_t* gxpci_context = &gxpci_context_body;
  void *buf_mem = t2h_page[queue_index / PAGE_SHARER] +
                  (MAP_LENGTH / PAGE_SHARER) * (queue_index % PAGE_SHARER);

  // Bind to the rank'th tile in the cpu set.
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, rank)) < 0)
	  tmc_task_die("tmc_cpus_set_my_cpu() failed.");
  
  printf("RD thread_send(%d): queue_index %d\n", rank, queue_index);
  
  result = gxpci_open_raw_dma_send_queue(trio_context, rd_state,
                                         gxpci_context, queue_index);
  VERIFY_ZERO(result, "gxpci_open_raw_dma_send_queue()");

  host_buf_size = gxpci_raw_dma_get_host_buf_size(gxpci_context);

  printf("thread_send(%d): T2H queue opened, host buffer size %#x, "
         "waiting to send ...\n", rank, host_buf_size);

  // Run the test.
  do_send(gxpci_context, buf_mem);
  return (void*)NULL;
}

int main(int argc, char**argv)
{
  void *buf_mem;
  int result;

  parse_args(argc, argv);
  
  //
  // Initialize TRIO context and get a TRIO asid.
  //
  result = gxio_trio_init(trio_context, trio_index);
  VERIFY_ZERO(result, "gxio_trio_init()");
  trio_asid = gxio_trio_alloc_asids(trio_context, 1, 0, 0);
  if (trio_asid < 0)
  {
    tmc_task_die("Failure in gxio_trio_alloc_asids(), asid = %d", trio_asid);
  }

  //
  // Initialize the Raw DMA state structure.
  //
  result = gxpci_raw_dma_init(trio_context, rd_state, trio_index,
                              loc_mac, trio_asid);
  VERIFY_ZERO(result, "gxpci_raw_dma_init()");

  num_senders = rd_state->num_rd_t2h_queues;

  //
  // It's good practice to reset the ASID value because gxpci_raw_dma_init()
  // allocates a new ASID if it is passed a GXIO_ASID_NULL.
  trio_asid = rd_state->asid;

  //
  // Allocate and register data buffers.
  //
  for (int i = 0; i < T2H_HUGE_PAGES; i ++)
  {
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_huge(&alloc);
    buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
    assert(buf_mem);

    result = gxio_trio_register_page(trio_context, trio_asid, buf_mem,
                                     MAP_LENGTH, 0);
    VERIFY_ZERO(result, "gxio_trio_register_page()");

    t2h_page[i] = buf_mem;
  }

  //
  // Start pthreads to send data.
  //
  if (tmc_cpus_get_my_affinity(&desired_cpus) != 0)
     tmc_task_die("tmc_cpus_get_my_affinity() failed.");
  
  pthread_t send_threads[num_senders];
  
  for (int i = 0; i < num_senders; i ++)
  {
     if (pthread_create(&send_threads[i], NULL, thread_send,
         (void*)(intptr_t)i) != 0)
	tmc_task_die("pthread_create() failed.");
  }
  
  for (int i = 0; i < num_senders; i++)
  {
     if (pthread_join(send_threads[i], NULL) != 0)
     {
	tmc_task_die("thread_join() failed.");
     }
  }

  gxpci_raw_dma_destroy(trio_context, rd_state);

  return 0;
}
