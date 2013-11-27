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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arch/sim.h>
#include <arch/atomic.h>

#include <gxio/mpipe.h>

#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/mem.h>
#include <tmc/sync.h>
#include <tmc/task.h>


// Align "p" mod "align", assuming "p" is a "void*".
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)


// Help check for errors.
#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)



// Total number of working tiles.
#define NUM_WORKERS     (6)

// Total number of Notification Groups.
#define NUM_NOTIFGROUPS (3)

// Total number of Buckets.
#define NUM_BUCKETS     (1 + 2 + 4)


// Initial affinity.
static cpu_set_t cpus;

// See below.
static gxio_mpipe_rules_dmac_t dmac0 =
  {{ 0x00, 0x11, 0x43, 0xD2, 0x47, 0x0D }};

// The "input.pcap" file has 1000 packets, 0 of which use vlan 0 to 9,
// and 574 of which use dmac 0 (above).  We clone this file onto channels
// 0, 1, and 2, and we have one group of workers processing each channel,
// using different filters and packet distribution mechanisms.
static int num_pkt_total = 1000;
static int num_pkt_vlan = 0;
static int num_pkt_dmac0 = 574;

// Threads for all workers.
static pthread_t threads[NUM_WORKERS];

// Number of packets received by each worker.
static int packets[NUM_WORKERS] = { 0 };

// Barrier for all workers.
static tmc_sync_barrier_t barrier = TMC_SYNC_BARRIER_INIT(NUM_WORKERS);

// The mPIPE instance.
static int instance;

// The mPIPE context (shared by all workers).
static gxio_mpipe_context_t context_body;
static gxio_mpipe_context_t* context = &context_body;

// The mPIPE iqueues, one per worker.
static gxio_mpipe_iqueue_t* iqueues[NUM_WORKERS];


// Process on ingress packets after classification.
//
void
process_packets(int rank)
{
  gxio_mpipe_iqueue_t* iqueue = iqueues[rank];

  gxio_mpipe_idesc_t idesc;

  // Wait for a packet until we have enough packet number as input.
  while (1)
  {
    // Try to get one packet from the tile's iqueue.
    int result = gxio_mpipe_iqueue_try_get(iqueue, &idesc);

    // There is one ingress packet.
    if (result == 0)
    {
      if (!gxio_mpipe_iqueue_drop_if_bad(iqueue, &idesc))
      {
        // TODO: User would normally do something with the packet here.

        // Just drop the packet.
        gxio_mpipe_iqueue_drop(iqueue, &idesc);
      }

      // Count the packets handled by this worker.
      packets[rank]++;
    }

    // Detect a real error.
    if (result != GXIO_MPIPE_ERR_IQUEUE_EMPTY)
      VERIFY(result, "gxio_mpipe_iqueue_try_get()");

    // Break when the group has handled sufficient packets.

    // Group 1 workers should have num_pkt_vlan packets total.
    if (rank == 0 || rank == 1)
    {
      if (packets[0] + packets[1] == num_pkt_vlan)
        break;
    }
    // Group 2 workers should have num_pkt_dmac0 packets total.
    else if (rank == 2 || rank == 3)
    {
      if (packets[2] + packets[3] == num_pkt_dmac0)
        break;
    }
    // Group 3 workers should have num_pkt_total packets total.
    else
    {
      int total = 0;
      for (int i = 4; i < NUM_WORKERS; i++)
        total += packets[i];

      if (total == num_pkt_total)
        break;
    }

    // Avoid reading packets[rank] from registers.
    arch_atomic_compiler_barrier();
  }

  printf("Cpu %d handled %4d packets.\n",
         tmc_cpus_find_nth_cpu(&cpus, rank), packets[rank]);
}


// Function for each worker tile.
//
static void*
thread_func(void* arg)
{
  int rank = (intptr_t)arg;

  // Bind current thread to a single cpu.
  int result = tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank));
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  // Sync for all workers.
  tmc_sync_barrier_wait(&barrier);

  if (rank == 0)
  {
    // Pause briefly, to let everyone finish passing the barrier.
    for (int i = 0; i < 5000; i++)
      __insn_mfspr(SPR_PASS);

    // Allow packets to flow.
    sim_enable_mpipe_links(instance, -1);
  }

  // Work on each ingress packets.
  process_packets(rank);

  // Sync for all workers.
  tmc_sync_barrier_wait(&barrier);

  return (void*)NULL;
}


// Main function.
//
int
main(int argc, char** argv)
{
  // Determine the available cpus.
  int result = tmc_cpus_get_my_affinity(&cpus);
  VERIFY(result, "tmc_cpus_get_my_affinity()");

  // Check available cpu number.
  if (tmc_cpus_count(&cpus) < NUM_WORKERS)
    tmc_task_die("Insufficient cpus.");

  int instance = gxio_mpipe_link_instance("gbe1");
  assert(instance == gxio_mpipe_link_instance("gbe2"));
  assert(instance == gxio_mpipe_link_instance("gbe3"));

  // Start the driver.
  result = gxio_mpipe_init(context, instance);
  VERIFY(result, "gxio_mpipe_init()");

  gxio_mpipe_link_t link0;
  result = gxio_mpipe_link_open(&link0, context, "gbe1", 0);
  VERIFY(result, "gxio_mpipe_link_open()");

  gxio_mpipe_link_t link1;
  result = gxio_mpipe_link_open(&link1, context, "gbe2", 0);
  VERIFY(result, "gxio_mpipe_link_open()");

  gxio_mpipe_link_t link2;
  result = gxio_mpipe_link_open(&link2, context, "gbe3", 0);
  VERIFY(result, "gxio_mpipe_link_open()");

  int channel0 = gxio_mpipe_link_channel(&link0);
  int channel1 = gxio_mpipe_link_channel(&link1);
  int channel2 = gxio_mpipe_link_channel(&link2);


  // Allocate and init one NotifRing for each worker.
  result = gxio_mpipe_alloc_notif_rings(context, NUM_WORKERS, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_rings()");
  int ring = result;

  for (int i = 0; i < NUM_WORKERS; i++)
  {
    // Home each iqueue on its worker's home tile.
    tmc_alloc_t alloc0 = TMC_ALLOC_INIT;
    tmc_alloc_set_home(&alloc0, tmc_cpus_find_nth_cpu(&cpus, i));

    // Allocate one iqueue for each worker.
    gxio_mpipe_iqueue_t* iqueue =
      tmc_alloc_map(&alloc0, sizeof(gxio_mpipe_iqueue_t));
    if (iqueue == NULL)
      tmc_task_die("Failure in 'tmc_alloc_map()'.");
    iqueues[i] = iqueue;

    // Total number of entries for a NotifRing.
    size_t notif_ring_entries = 512;

    // Total number of bytes for a NotifRing.
    size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);

    // Allocation attributes for NotifRings.
    if (!tmc_alloc_set_pagesize(&alloc0, notif_ring_size))
      tmc_task_die("No page big enough for %zd bytes\n", notif_ring_size);

    // Allocate one NotifRing for each worker.
    void* iqueue_mem = tmc_alloc_map(&alloc0, notif_ring_size);
    if (iqueue_mem == NULL)
      tmc_task_die("Failure in 'tmc_alloc_map()'.");

    // Init one NotifRing for each worker.
    result = gxio_mpipe_iqueue_init(iqueue, context, ring + i,
                                    iqueue_mem, notif_ring_size, 0);
    VERIFY(result, "gxio_mpipe_iqueue_init()");
  }

  // Allocate NUM_NOTIFGROUPS NotifRing groups.
  result = gxio_mpipe_alloc_notif_groups(context, NUM_NOTIFGROUPS, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_groups()");
  int group = result;

  // Allocate NUM_BUCKETS buckets.
  result = gxio_mpipe_alloc_buckets(context, NUM_BUCKETS, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  int bucket = result;

  // Total number of buffers in a stack.
  unsigned int num_buffers = 512;

  // Max size per packet in enum.
  gxio_mpipe_buffer_size_enum_t buf_size = GXIO_MPIPE_BUFFER_SIZE_1664;

  // Allocate one buffer stack.
  result = gxio_mpipe_alloc_buffer_stacks(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buffer_stacks()");
  int stack_idx = result;

  // Allocate one huge page to hold the buffer stack, and all the
  // buffers.  This should be more than enough space.
  size_t page_size = tmc_alloc_get_huge_pagesize();
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* page = tmc_alloc_map(&alloc, page_size);
  assert(page != NULL);
  void* mem = page;

  // Initialize the buffer stack.  Must be aligned mod 64K.
  ALIGN(mem, 0x10000);
  size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(num_buffers);
  result = gxio_mpipe_init_buffer_stack(context, stack_idx, buf_size,
                                        mem, stack_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  mem += stack_bytes;

  ALIGN(mem, 0x10000);

  // Register the entire huge page of memory which contains all buffers.
  result = gxio_mpipe_register_page(context, stack_idx, page, page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");

  // Push some buffers into the stack.
  for (int i = 0; i < num_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx, mem);
    mem += 1664;
  }


  gxio_mpipe_rules_t rules;
  gxio_mpipe_rules_init(&rules, context);

  // Group 0 uses bucket 0 to round robin to rings 0 and 1.
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_ROUND_ROBIN;
  result = gxio_mpipe_init_notif_group_and_buckets(context, group + 0,
                                                   ring + 0, 2,
                                                   bucket + 0, 1, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");

  // Bucket 0 handles traffic from channel 0 with vlan 0 to 9.
  // ISSUE: There is no such traffic in "input.pcap".
  gxio_mpipe_rules_begin(&rules, bucket + 0, 1, NULL);
  gxio_mpipe_rules_add_channel(&rules, channel0);
  for (int i = 0; i < 10; i++)
    gxio_mpipe_rules_add_vlan(&rules, i);

  // Group 1 uses bucket 1 for ring 2, and bucket 2 for ring 3.
  mode = GXIO_MPIPE_BUCKET_STATIC_FLOW_AFFINITY;
  result = gxio_mpipe_init_notif_group_and_buckets(context, group + 1,
                                                   ring + 2, 2,
                                                   bucket + 1, 2, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");

  // Buckets 1 and 2 handle traffic from channel 1 with dmac 0.
  // ISSUE: 55/574 packets in "input.pcap" go to bucket 1.
  gxio_mpipe_rules_begin(&rules, bucket + 1, 2, NULL);
  gxio_mpipe_rules_add_channel(&rules, channel1);
  gxio_mpipe_rules_add_dmac(&rules, dmac0);

  // Group 2 uses buckets 3 to 6 with dynamic flow to rings 4 and 5.
  // ISSUE: Usually, but not always, 500 packets go to each ring.
  mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
  result = gxio_mpipe_init_notif_group_and_buckets(context, group + 2,
                                                   ring + 4, NUM_WORKERS - 4,
                                                   bucket + 3,
                                                   NUM_BUCKETS - 3, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");

  // Buckets 3 to 6 handle all traffic from channel 2.
  gxio_mpipe_rules_begin(&rules, bucket + 3, NUM_BUCKETS - 3, NULL);
  gxio_mpipe_rules_add_channel(&rules, channel2);

  // Commmit all rules.
  result = gxio_mpipe_rules_commit(&rules);
  VERIFY(result, "gxio_mpipe_rules_commit()");


  // Spawn a thread on each tile.
  for (int i = 0; i < NUM_WORKERS; i++)
  {
    if (pthread_create(&threads[i], NULL,
                       thread_func, (void*)(intptr_t)i) != 0)
      tmc_task_die("Failure in 'pthread_create()'.");
  }

  // Wait for all threads to be finished.
  for (int i = 0; i < NUM_WORKERS; i++)
  {
    if (pthread_join(threads[i], NULL) != 0)
      tmc_task_die("Failure in 'pthread_join()'.");
  }

  return 0;
}
