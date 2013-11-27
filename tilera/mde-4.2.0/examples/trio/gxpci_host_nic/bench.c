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
// Measure data transfer throughput between the Tile and Host.
//
// Tile:
// - If "dont_receive" is set, then the receiver thread just open the 
//   H2T queue, it does not transder data.
//
// Host: 
// - A "echo" module is needed, it will cooperate with the Tile side
//   application, to either send back the received data to Tile, or
//   just drop the packet and frees the buffer.
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

#include <arch/cycle.h>

#include <tmc/perf.h>

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxpci_strerror(__val));       \
  } while (0)

// Since each NIC can have max 15 I/O TLB entries for data buffers,
// we allocate 8 huge pages here.
#define T2H_HUGE_PAGES		4
#define H2T_HUGE_PAGES		4

// The packet pool memory size is 8 huge pages,
// 4 for T2H queues and 4 for H2T queues.
void *t2h_page[T2H_HUGE_PAGES], *h2t_page[T2H_HUGE_PAGES];

#define MAP_LENGTH 		(16 * 1024 * 1024)

#define MAX_PKT_SIZE		2048

#define TOTAL_BYTES_ORDER	28

#define MAX_CMDS_BATCH		16

// Since max number of T2H and H2T queues is 4, 
// each queue uses a quarter of the huge page.
#define PKTS_IN_POOL		((MAP_LENGTH >> 2) / MAX_PKT_SIZE)

gxio_trio_context_t trio_context_body;
gxio_trio_context_t* trio_context = &trio_context_body;

gxpci_nic_state_t nic_state_body;
gxpci_nic_state_t *nic_state = &nic_state_body;

cpu_set_t desired_cpus;

// The TRIO index.
int trio_index = 0;
int trio_asid = 0;

// The local MAC index.
int loc_mac = 0;

// Flag indicating that the sender has sent all the packets.
int send_done = 0;

size_t packet_size = 1344;
int num_senders;
int num_receivers;
int dont_recv = 0;	// by default, we will receive data from the Host

int nic_index = 0;	// by default, we will talk to NIC index 0.

// Sync variable
tmc_sync_barrier_t barrier;
tmc_sync_barrier_t recv_barrier;

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
    else if ((opt = shift_option(&args, "--nic_index=")))
      nic_index = atoi(opt);
    else if ((opt = shift_option(&args, "--dontrecv=")))
      dont_recv = atoi(opt); 
    else
    {
      fprintf(stderr, "Unknown option '%s'.\n", args[0]);
      fprintf(stderr,
              "Usage: bench --mac=0 --size=1344 --nic_index=0 --dontrecv=0\n");
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
#ifdef T2H_DEBUG
  unsigned recv_seq_num = 0;
#endif
  unsigned long cmds_total = (1ULL << TOTAL_BYTES_ORDER) / packet_size;
  int cpu;
  int result;
  gxpci_nic_comp_t comp[MAX_CMDS_BATCH];
  gxpci_cmd_t cmd[MAX_CMDS_BATCH];
  gxpci_cmd_t template = {
    .buffer = NULL,
    .size = packet_size + sizeof(tile_nic_tile_desc_t),
  };
 
  tile_nic_tile_desc_t *t2h_header;

  for (int i = 0; i < MAX_CMDS_BATCH; i++)
    cmd[i] = template;

  cpu = tmc_cpus_get_my_current_cpu();

  printf("do_send() started on tile %d, total = %lu\n", cpu, cmds_total);

  start_cycles = get_cycle_count();
  
  while (!send_done)
  {
    if (seq_num < cmds_total)
    {
      unsigned credits = gxpci_nic_get_cmd_credits(context);
      unsigned count = MIN(MAX_CMDS_BATCH, credits);

      count = MIN(count, (cmds_total - seq_num));

      // Post the next 'count' buffers.
      for (int i = 0; i < count; i++)
      {
        cmd[i].buffer = buf_mem +
          ((send_seq_num + i) % PKTS_IN_POOL) * MAX_PKT_SIZE;

        // Save the pkt size in our custom T2H header.
        t2h_header = (tile_nic_tile_desc_t *)cmd[i].buffer;
        t2h_header->size = packet_size;

        // Real data starts at (buffer + sizeof(tile_nic_tile_desc_t)).

        // Make data visible to TRIO.
        __insn_mf();

        result = gxpci_nic_t2h_cmd(context, &cmd[i]);
        if (result == GXPCI_ERESET)
        {
          printf("do_send: channel is reset\n");
          goto send_reset;
        }
        VERIFY_ZERO(result, "gxpci_nic_t2h_cmd()");
      }
      send_seq_num += count;
    }

    int comp_count = gxpci_nic_get_comps(context, comp, 0, MAX_CMDS_BATCH);
    if (comp_count == GXPCI_ERESET)
    {
      printf("do_send: channel is reset\n");
      goto send_reset;
    }
#ifdef T2H_DEBUG
    else
    {
      for (int i = 0; i < comp_count; i++)
      {
        void *expected_buffer = buf_mem +
          ((recv_seq_num + i) % PKTS_IN_POOL) * MAX_PKT_SIZE;

        if (expected_buffer != comp[i].buffer ||
            (packet_size + sizeof(tile_nic_tile_desc_t)) != comp[i].desc.size)
        {
          printf("comp mismatch: pkt %u buffer exp %p %p size exp %lu %u\n",
                 recv_seq_num + i, expected_buffer, comp[i].buffer,
                 packet_size + sizeof(tile_nic_tile_desc_t), comp[i].desc.size);
          goto send_reset;
        }
      }
      recv_seq_num += comp_count;
    }
#endif

    seq_num += comp_count;

    if (seq_num >= cmds_total)
      send_done = 1;
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
}

void *thread_send(void* arg)
{
  int result;
  int rank = (intptr_t) arg;
  int queue_index = rank;
  gxpci_context_t gxpci_context_body;
  gxpci_context_t* gxpci_context = &gxpci_context_body;
  void *buf_mem = t2h_page[queue_index / 4] +
                  (MAP_LENGTH >> 2) * (queue_index % 4);

  // Bind to the rank'th tile in the cpu set
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, rank)) < 0)
	  tmc_task_die("tmc_cpus_set_my_cpu() failed.");
  
  printf("thread_send(%d): queue_index %d\n", rank, queue_index);
  
  result = gxpci_nic_t2h_queue_init(trio_context, nic_state,
                                    gxpci_context, queue_index);
  VERIFY_ZERO(result, "gxpci_nic_t2h_queue_init()");

  result = gxpci_nic_t2h_queue_complete_init(gxpci_context);
  VERIFY_ZERO(result, "gxpci_nic_t2h_queue_complete_init()");

  printf("thread_send(%d): T2H queue opened, waiting to send ...\n", rank);
  tmc_sync_barrier_wait(&barrier);
  
  // Run the test.
  do_send(gxpci_context, buf_mem);
  return (void*)NULL;
}

void do_recv(gxpci_context_t* context, void* buf_mem)
{
  uint64_t start_cycles = 0;
  uint64_t finish_cycles = 0;
  unsigned int seq_num = 0;
  unsigned recv_seq_num = 0;
  unsigned long cmds_total = (1ULL << TOTAL_BYTES_ORDER) / packet_size;
  int cpu;
  int result;
  gxpci_nic_comp_t comp[MAX_CMDS_BATCH];
  gxpci_cmd_t cmd[MAX_CMDS_BATCH];
  gxpci_cmd_t template = {
    .buffer = NULL,
    .size = packet_size,
  };

  for (int i = 0; i < MAX_CMDS_BATCH; i++)
    cmd[i] = template;

  cpu = tmc_cpus_get_my_current_cpu();

  printf("do_recv() started on tile %d, total = %lu\n", cpu, cmds_total);
  
  while (!send_done)
  {
    if (seq_num < cmds_total)
    {
      unsigned credits = gxpci_nic_get_cmd_credits(context);
      unsigned count = MIN(MAX_CMDS_BATCH, credits);

      count = MIN(count, (cmds_total - seq_num));

      // Post the next 'count' buffers.
      for (int i = 0; i < count; i++)
      {
        cmd[i].buffer = buf_mem +
          ((recv_seq_num + i) % PKTS_IN_POOL) * MAX_PKT_SIZE;
        result = gxpci_nic_h2t_cmd(context, &cmd[i]);
        if (result == GXPCI_ERESET)
        {
          printf("do_recv: channel is reset\n");
          goto recv_reset;
        }
        VERIFY_ZERO(result, "gxpci_nic_h2t_cmd()");
      }
      recv_seq_num += count;
    }

    if (seq_num == 0)
      start_cycles = get_cycle_count();

    int comp_count = gxpci_nic_get_comps(context, comp, 0, MAX_CMDS_BATCH);

    if (comp_count == GXPCI_ERESET)
    {
      printf("do_recv: channel is reset\n");
      goto recv_reset;
    }

    seq_num += comp_count;
  }

recv_reset:

  finish_cycles = get_cycle_count();
  // All done.  Print the performance results to stdout.
  float gigabits = (float)seq_num * packet_size * 8;
  float cycles = finish_cycles - start_cycles;
  float pps = (float)seq_num / cycles * tmc_perf_get_cpu_speed();
  float gbps = gigabits / cycles * tmc_perf_get_cpu_speed() / 1e9;
  printf("thread_recv (%d): Received %u %d-byte packets: %f gbps, %f pps\n",
	 cpu, seq_num, (int)packet_size, gbps, pps);

  tmc_sync_barrier_wait(&recv_barrier);
}

void *thread_recv(void *arg)
{
  int result;
  int rank = (intptr_t) arg;
  int queue_index = rank;
  gxpci_context_t gxpci_context_body;
  gxpci_context_t* gxpci_context = &gxpci_context_body;
  void *buf_mem = h2t_page[queue_index / 4] +
                  (MAP_LENGTH >> 2) * (queue_index % 4);

  rank += num_senders;

  // Bind to the rank'th tile in the cpu set (RECV)
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&desired_cpus, rank)) < 0)
	  tmc_task_die("tmc_cpus_set_my_cpu() failed.");
  
  printf("thread_recv(%d): queue_index %d\n", rank, queue_index);
  
  result = gxpci_nic_h2t_queue_init(trio_context, nic_state,
                                    gxpci_context, queue_index);
  VERIFY_ZERO(result, "gxpci_nic_h2t_queue_init()");

  result = gxpci_nic_h2t_queue_complete_init(gxpci_context);
  VERIFY_ZERO(result, "gxpci_nic_h2t_queue_complete_init()");
  
  if (dont_recv)
  {
	printf("thread_recv(%d): H2T queue opened, waiting to exit ...\n",
               rank);
	tmc_sync_barrier_wait(&barrier);
  } else {
	printf("thread_recv(%d): H2T queue opened, waiting to receive ...\n",
               rank);
	tmc_sync_barrier_wait(&barrier);
  
  	// Run the test.
  	do_recv(gxpci_context, buf_mem);
  }

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
  // Initialize the NIC state structure.
  //
  result = gxpci_nic_init(trio_context, nic_state, trio_index,
                          loc_mac, nic_index, trio_asid);
  VERIFY_ZERO(result, "gxio_nic_init()");

  num_senders = nic_state->num_nic_tx_queues;
  num_receivers = nic_state->num_nic_rx_queues;

  //
  // It's good practice to reset the ASID value because gxpci_nic_init()
  // allocates a new ASID if it is passed a GXIO_ASID_NULL.
  trio_asid = nic_state->asid;

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

  for (int i = 0; i < H2T_HUGE_PAGES; i ++)
  {
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_huge(&alloc);
    buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
    assert(buf_mem);

    result = gxio_trio_register_page(trio_context, trio_asid, buf_mem, 
                                     MAP_LENGTH, 0);
    VERIFY_ZERO(result, "gxio_trio_register_page()");

    h2t_page[i] = buf_mem;
  }

  //
  // Start pthreads to send and receive data.
  //
  tmc_sync_barrier_init(&barrier, num_receivers + num_senders);
  tmc_sync_barrier_init(&recv_barrier, num_receivers);
  
  if (tmc_cpus_get_my_affinity(&desired_cpus) != 0)
     tmc_task_die("tmc_cpus_get_my_affinity() failed.");
  
  pthread_t send_threads[num_senders];
  pthread_t recv_threads[num_receivers];
  
  for (int i = 0; i < num_senders; i ++)
  {
     if (pthread_create(&send_threads[i], NULL, thread_send,
         (void*)(intptr_t)i) != 0)
	tmc_task_die("pthread_create() failed.");
  }
  
  for (int i = 0; i < num_receivers; i++)
  {    
     if (pthread_create(&recv_threads[i], NULL, thread_recv,
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

  for (int i = 0; i < num_receivers; i++) 
  {
     if (pthread_join(recv_threads[i], NULL) != 0)
     {
	tmc_task_die("thread_join() failed.");
     }
  }
  
  gxpci_nic_destroy(trio_context, nic_state);

  return 0;
}
