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

#include <gxio/trio.h>
#include <gxpci/gxpci.h>
#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/perf.h>
#include <tmc/task.h>
#include <tmc/spin.h>

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxpci_strerror(__val));       \
  } while (0)

// Number of threads.
#define NUM_THREADS		2

// The packet pool memory size.
#define MAP_LENGTH 		(16 * 1024 * 1024)

#if 0
#define MAX_PKT_SIZE		(4 * 1024)
#else
#define MAX_PKT_SIZE		64
#endif

#define MAX_CMDS_BATCH		256

#if 0
#define DEBUG
#endif

#if 0
#define CHECK_DATA_PATTERN
#define SEND_DATA_PATTERN
#endif

// The TRIO index.
int trio_index = 0;

// The queue index of a host queue, used by both sender and receiver.
int queue_index = 0;

// The queue type, physical function by default.
gxpci_queue_type_t queue_type = GXPCI_PQ_DUPLEX;

// The virtual function number.
int vf_instance;

// The local MAC index.
int loc_mac;

unsigned int pkt_size = MAX_PKT_SIZE;

int cpu_rank = 5;

// Contexts for H2T and T2H Packet Queues.
gxpci_context_t h2t_context_body;
gxpci_context_t t2h_context_body;

gxpci_context_t *h2t_context = &h2t_context_body;
gxpci_context_t *t2h_context = &t2h_context_body;

pthread_t threads[NUM_THREADS];
tmc_spin_barrier_t barrier;
cpu_set_t desired_cpus;

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
void parse_args(int argc, char** argv)
{
  char **args = &argv[1];

  // Scan options.
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--mac=")))
      loc_mac = atoi(opt);
    else if ((opt = shift_option(&args, "--pkt_size=")))
      pkt_size = atoi(opt);
    else if ((opt = shift_option(&args, "--cpu_rank=")))
      cpu_rank = atoi(opt);
    else if ((opt = shift_option(&args, "--queue_index=")))
      queue_index = atoi(opt);
    else if ((opt = shift_option(&args, "--vf=")))
    {
      queue_type = GXPCI_PQ_DUPLEX_VF;
      vf_instance = atoi(opt);
    }
    else
    {
      fprintf(stderr, "Tile: Unknown option '%s'.\n", args[0]);
      exit(EXIT_FAILURE);
    }
  }
}

void do_send(gxpci_context_t* context, void* buf_mem)
{
  gxpci_comp_t comp[MAX_CMDS_BATCH];
  gxpci_cmd_t cmd[MAX_CMDS_BATCH];
  gxpci_cmd_t template = {
    .buffer = NULL,
    .size = pkt_size,
  };
  unsigned int seq_num = 0;
  int pkt_size_order;
  int result;
  int done = 0;
#ifdef STATS
  int comps_rcvd = 0;
  int scomps_rcvd = 0;
  int ols = 0;
  int min_comps = MAX_CMDS_BATCH;
  int max_comps = 0;
  int min_creds = 1000000;
  int max_creds = 0;
  int tot_cred = 0;
#endif
#ifdef SEND_DATA_PATTERN
  uint32_t data_pattern = 0;
  int j;
#endif
  for (int i = 0; i < MAX_CMDS_BATCH; i++)
    cmd[i] = template;

#ifdef DEBUG
  printf("Tile: t2h %d: starting up...\n", tmc_cpus_get_my_current_cpu());
#endif

  if (!pkt_size)
  {
    fprintf(stderr, "Tile: Bad packet size 0.\n");
    exit(EXIT_FAILURE);
  }

  if (pkt_size & (pkt_size - 1))
    pkt_size_order = 32 - __builtin_clz(pkt_size);
  else
    pkt_size_order = __builtin_ctz(pkt_size);

  while (!done)
  {
    int credits = gxpci_get_cmd_credits(context);
    if (credits == GXPCI_ERESET)
    {
      printf("Tile: do_send: channel is reset %u\n", seq_num);
      done = 1;
      break;
    }

#ifdef STATS
    if ((comps_rcvd > STATS_START) && comps_rcvd < STATS_STOP) 
    {
      if (credits > max_creds)
        max_creds = credits;
      if (credits < min_creds)
        min_creds = credits;

      tot_cred += credits;
    }
#endif
    
    unsigned count = MIN(MAX_CMDS_BATCH, credits);

    // Post the next 'count' buffers.
    for (int i = 0; i < count; i++)
    {
      cmd[i].buffer =
        buf_mem + ((seq_num << pkt_size_order) & (MAP_LENGTH - 1));

#ifdef SEND_DATA_PATTERN
      for (j = 0; j < pkt_size; j += 4)
      {
        *((uint32_t*) (cmd[i].buffer + j)) = data_pattern++;
      }
      // Make data visible to push DMA command.
      __insn_mf();
#endif

      result = gxpci_pq_t2h_cmd(context, &cmd[i]);

#ifdef DEBUG
      printf("t2h: post seq %u size %u cmd_buffer %p\n",
             seq_num, pkt_size, cmd[i].buffer);
#endif
      seq_num++;
    }

    result = gxpci_get_comps(context, comp, 0, MAX_CMDS_BATCH);
    if (result == GXPCI_ERESET)
    {
      printf("Tile: do_send %d: channel is reset %u xfers\n", 
             tmc_cpus_get_my_current_cpu(), seq_num);
      done = 1;
      break;
    }
    else {
#ifdef STATS
      comps_rcvd += result;
      if ((comps_rcvd > STATS_START) && comps_rcvd < STATS_STOP) 
      {
        scomps_rcvd += result;
        ols++;
        if (result < min_comps)
          min_comps = result;
        if (result > max_comps)
          max_comps = result;
      }
#endif
    }
  }
#ifdef STATS
  printf("do_send %d completion stats: Total=%d, Max=%d, Min=%d, Avg=%0.2f\n",
         tmc_cpus_get_my_current_cpu(), scomps_rcvd, max_comps, min_comps,
         (float) scomps_rcvd / (float) ols);
  printf("do_send %d credit stats: Total=%d, Max=%d, Min=%d, Avg=%0.2f\n",
         cpu_rank, tot_cred, max_creds, min_creds, 
         (float) tot_cred / (float) ols);
  printf("t2h %d: host completed %d xfers\n",
         tmc_cpus_get_my_current_cpu(), comps_rcvd);
#endif
}

void do_recv(gxpci_context_t* context, void* buf_mem)
{
  gxpci_comp_t comp[MAX_CMDS_BATCH];
  gxpci_cmd_t cmd[MAX_CMDS_BATCH];
  gxpci_cmd_t template = {
    .buffer = NULL,
    .size = pkt_size,
  };
  unsigned int seq_num = 0;
  int pkt_size_order;
  int result;
#ifdef CHECK_DATA_PATTERN
  uint32_t actual_data = 0;
  uint32_t expected_data = 0;  
  uint32_t* chk_buf;
#endif

#ifdef DEBUG
  printf("h2t %d: starting up...\n", tmc_cpus_get_my_current_cpu());
#endif

  if (!pkt_size)
  {
    fprintf(stderr, "Tile: Bad packet size 0.\n");
    exit(EXIT_FAILURE);
  }

  if (pkt_size & (pkt_size - 1))
    pkt_size_order = 32 - __builtin_clz(pkt_size);
  else
    pkt_size_order = __builtin_ctz(pkt_size);

  for (int i = 0; i < MAX_CMDS_BATCH; i++)
    cmd[i] = template;

  while (1)
  {
    unsigned credits = gxpci_get_cmd_credits(context);
    unsigned count = MIN(MAX_CMDS_BATCH, credits);

    // Post the next 'count' buffers.
    for (int i = 0; i < count; i++)
    {
      cmd[i].buffer =
        buf_mem + ((seq_num << pkt_size_order) & (MAP_LENGTH - 1));

      seq_num++;

      result = gxpci_pq_h2t_cmd(context, &cmd[i]);
    }

    result = gxpci_get_comps(context, comp, 0, MAX_CMDS_BATCH);
    if (result == GXPCI_ERESET)
    {
      printf("Tile: do_recv: channel is reset %u\n", seq_num);
      break;
    }
#ifdef CHECK_DATA_PATTERN
    for (int i = 0; i < result; i++)
    {
      chk_buf = (uint32_t *) (comp[i].buffer);
      for (int j = 0; j < (pkt_size >> 2); j++) 
      {
        actual_data = chk_buf[j];
        if (actual_data != expected_data)
        {
          printf("Tile: h2t %d: expected %d but got %d on completion %d\n",
                 tmc_cpus_get_my_current_cpu(), expected_data, actual_data, i);
          goto done;
        }
        expected_data++;
      }
    }
#endif
  }

#ifdef CHECK_DATA_PATTERN
done:
  printf("Tile: h2t %d: checked %u words (mod 4G)\n",
         tmc_cpus_get_my_current_cpu(), expected_data);
#endif
}

void* thread_func(void *arg)
{
  int rank = (intptr_t) arg;
  int cpu;

  if (rank == 0)
    cpu = tmc_cpus_find_nth_cpu(&desired_cpus, cpu_rank);
  else
    cpu = tmc_cpus_find_nth_cpu(&desired_cpus, cpu_rank + 1);

  if (tmc_cpus_set_my_cpu(cpu) < 0)
    tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");

  if (rank == 0)
  {
    if (queue_type == GXPCI_PQ_DUPLEX)
      printf("Tile PF: H2T running on rank %d\n", cpu);
    else
      printf("Tile VF %d: H2T running on rank %d\n", vf_instance, cpu);

    // Allocate and register data buffers.
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_huge(&alloc);
    void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
    assert(buf_mem);

    int result = gxpci_iomem_register(h2t_context, buf_mem, MAP_LENGTH);
    VERIFY_ZERO(result, "gxpci_iomem_register()");

    tmc_spin_barrier_wait(&barrier);

    // Run the test.
    do_recv(h2t_context, buf_mem);
  }
  else if (rank == 1)
  {
    if (queue_type == GXPCI_PQ_DUPLEX)
      printf("Tile PF: T2H running on rank %d\n", cpu);
    else
      printf("Tile VF %d: T2H running on rank %d\n", vf_instance, cpu);

    // Allocate and register data buffers.
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_huge(&alloc);
    void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
    assert(buf_mem);

    int result = gxpci_iomem_register(t2h_context, buf_mem, MAP_LENGTH);
    VERIFY_ZERO(result, "gxpci_iomem_register()");

    tmc_spin_barrier_wait(&barrier);

    // Run the test.
    do_send(t2h_context, buf_mem);
  }

  return (void*) NULL;
}

// Function to create all threads.
void run_threads(int count, void*(*func)(void*))
{
  for (int i = 0; i < count; i++)
  {
    if (pthread_create(&threads[i], NULL, func, (void*)(intptr_t)i) != 0)
      tmc_task_die("pthread_create() failed.");
  }
}

// Function to collect all threads.
void finish_threads(int count)
{
  for (int i = 0; i < count; i++)
  {
    if (pthread_join(threads[i], NULL) != 0)
      tmc_task_die("pthread_join() failed.");
  }
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
     tmc_task_die("tmc_cpus_get_my_affinity() failed.");

  // Bind to the current Tile in the cpu set.
  int cpu = tmc_cpus_get_my_current_cpu();
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, cpu)) < 0)
     tmc_task_die("tmc_cpus_set_my_cpu() failed.");

  // Get a gxio context.
  int result = gxio_trio_init(trio_context, trio_index);
  VERIFY_ZERO(result, "gxio_trio_init()");

  result = gxpci_init(trio_context, h2t_context, trio_index, loc_mac);
  VERIFY_ZERO(result, "gxpci_init()");

  result = gxpci_init(trio_context, t2h_context, trio_index, loc_mac);
  VERIFY_ZERO(result, "gxpci_init()");

  result = gxpci_open_duplex_queue(h2t_context, t2h_context, asid,
                                   queue_type,
        	                   vf_instance,
                	           queue_index);
  VERIFY_ZERO(result, "gxpci_open_queue()");

  // Barrier init.
  tmc_spin_barrier_init(&barrier, NUM_THREADS);

  // Go parallel.
  run_threads(NUM_THREADS, thread_func);

  // Wait for all threads.
  finish_threads(NUM_THREADS);

  gxpci_destroy_duplex(h2t_context, t2h_context, queue_type);

  return 0;
}
