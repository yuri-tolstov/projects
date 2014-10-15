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
#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/perf.h>
#include <tmc/task.h>

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxpci_strerror(__val));       \
  } while (0)

// The packet pool memory size.
#define MAP_LENGTH 		(16 * 1024 * 1024)

// This can be changed to any size < 16KB as long as it fits in the host
// buf_size.
#if 0
#define MAX_PKT_SIZE		(4 * 1024)
#else
#define MAX_PKT_SIZE		64
#endif

#if 0
#define SEND_DATA_PATTERN
#endif

#if 0
#define DEBUG
#endif

#if 0
#define STATS
#endif

#define STATS_START             1000000
#define STATS_STOP              5000000
#define MAX_CMDS_BATCH          256

// The running cpu number.
int cpu_rank = 6;

// The queue type, physical function by default.
gxpci_queue_type_t queue_type = GXPCI_PQ_T2H;

// The virtual function number.
int vf_instance;

// The TRIO index.
int trio_index = 0;

// The queue index of a packet queue.
int queue_index;

// The local MAC index.
int loc_mac;

unsigned int pkt_size = MAX_PKT_SIZE;

static char* shift_option(char*** arglist, const char* option)
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
  char** args = &argv[1];

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
      queue_type = GXPCI_PQ_T2H_VF;
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
  printf("Tile: t2h %d: starting up...\n", cpu_rank);
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
             cpu_rank, seq_num);
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
         cpu_rank, scomps_rcvd, max_comps, min_comps, 
         (float) scomps_rcvd / (float) ols);
  printf("do_send %d credit stats: Total=%d, Max=%d, Min=%d, Avg=%0.2f\n",
         cpu_rank, tot_cred, max_creds, min_creds, 
         (float) tot_cred / (float) ols);
  printf("t2h %d: host completed %d xfers\n", cpu_rank, comps_rcvd);
#endif
  gxpci_destroy(context);
}

int main(int argc, char** argv)
{
  gxio_trio_context_t trio_context_body;
  gxio_trio_context_t* trio_context = &trio_context_body;
  gxpci_context_t gxpci_context_body;
  gxpci_context_t* gxpci_context = &gxpci_context_body;

  parse_args(argc, argv);

  // We must bind to a single CPU.
  cpu_set_t desired_cpus;
  if (tmc_cpus_get_my_affinity(&desired_cpus) != 0)
     tmc_task_die("tmc_cpus_get_my_affinity() failed.");

  // Bind to the cpu_rank'th Tile in the cpu set.
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, cpu_rank)) < 0)
     tmc_task_die("tmc_cpus_set_my_cpu() failed.");

  // This indicates that we need to allocate an ASID ourselves,
  // instead of using one that is allocated somewhere else.
  int asid = GXIO_ASID_NULL;

  if (queue_type == GXPCI_PQ_T2H)
    printf("Tile PF: t2h running on rank %d\n", cpu_rank);
  else
    printf("Tile VF %d: t2h running on rank %d\n", vf_instance, cpu_rank);


  // Get a gxio context.
  int result = gxio_trio_init(trio_context, trio_index);
  VERIFY_ZERO(result, "gxio_trio_init()");

  result = gxpci_init(trio_context, gxpci_context, trio_index, loc_mac);
  VERIFY_ZERO(result, "gxpci_init()");

  result = gxpci_open_queue(gxpci_context, asid,
                            queue_type,
                            vf_instance,
                            queue_index,
                            0,
                            0);
  VERIFY_ZERO(result, "gxpci_open_queue()");

  // Allocate and register data buffers.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
  assert(buf_mem);

  result = gxpci_iomem_register(gxpci_context, buf_mem, MAP_LENGTH);
  VERIFY_ZERO(result, "gxpci_iomem_register()");

  // Run the test.
  do_send(gxpci_context, buf_mem);
  
  return 0;
}
