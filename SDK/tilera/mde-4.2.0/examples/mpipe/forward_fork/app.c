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

#include <tmc/alloc.h>

#include <arch/atomic.h>
#include <arch/sim.h>

#include <gxio/mpipe.h>

#include <tmc/cpus.h>
#include <tmc/mem.h>
#include <tmc/sync.h>
#include <tmc/task.h>


// Align "p" mod "align", assuming "p" is a "void*".
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)


// The number of packets to process.
static int num_packets = 1000;

// The number of workers to use.
#ifdef NUM_WORKERS
static unsigned int num_workers = NUM_WORKERS;
#else
static unsigned int num_workers = 1;
#endif

// NOTE: We use the same number of buffers as there are entries in
// our edma ring, to avoid overflowing the edma ring, and to allow
// the packet sequence number to be used to preserve ordering.
// NOTE: 512 is not enough to handle (non-dataplane) interrupts.
static unsigned int num_buffers = 2048;

// The initial affinity.
static cpu_set_t cpus;

// The mPIPE instance.
static int instance;

typedef struct {

  // The mpipe context (shared by all workers).
  gxio_mpipe_context_t context;

  // The ingress queues (one per worker).
  gxio_mpipe_iqueue_t** iqueues;

  // The egress queue (shared by all workers).
  gxio_mpipe_equeue_t equeue;

  // Help synchronize process creation.
  tmc_sync_barrier_t barrier;

  // The total number of packets forwarded by all workers.
  volatile unsigned long total;

} shared_t;

static shared_t* shared;

// Pointer to "shared.context".
static gxio_mpipe_context_t* context;



// Help check for errors.
#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)



// The main function for each worker.
//
static void
main_aux(int rank)
{
  int result;


  // Bind to a single cpu.
  result = tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank));
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  tmc_sync_barrier_wait(&shared->barrier);

  if (rank == 0)
  {
    // HACK: Pause briefly, to let everyone finish passing the barrier.
    for (int i = 0; i < 10000; i++)
      __insn_mfspr(SPR_PASS);

    // Allow packets to flow.
    sim_enable_mpipe_links(instance, -1);
  }


  // Forward packets.

  gxio_mpipe_iqueue_t* iqueue = shared->iqueues[rank];

  while (num_packets < 0 || shared->total < num_packets)
  {
    // Wait for packets to arrive.

    gxio_mpipe_idesc_t* idesc;
    int n;

    if (num_packets < 0 || num_workers <= 1)
      n = gxio_mpipe_iqueue_peek(iqueue, &idesc);
    else
      n = gxio_mpipe_iqueue_try_peek(iqueue, &idesc);

    if (n > 16)
      n = 16;
    else if (n <= 0)
      continue;

    for (int i = 0; i < n; i++, idesc++)
    {
      // NOTE: We forward all packets, even "bad" ones.

      // TODO: Rewrite the packet header (unless "idesc->be")?

      // Prepare to forward.
      gxio_mpipe_edesc_t edesc;
      gxio_mpipe_edesc_copy_idesc(&edesc, idesc);

      // Make sure "be" packets just get dropped.
      edesc.ns = idesc->be;

      gxio_mpipe_equeue_put(&shared->equeue, edesc);

      // We are done with the packet.
      gxio_mpipe_iqueue_consume(iqueue, idesc);
    }

    // Count packets (atomically).
    arch_atomic_add(&shared->total, n);
  }
}


int
main(int argc, char** argv)
{
  int result;

  char* link_name = "gbe0";

  // Parse args.
  for (int i = 1; i < argc; i++)
  {
    char* arg = argv[i];

    if (!strcmp(arg, "--link") && i + 1 < argc)
    {
      link_name = argv[++i];
    }
    else if (!strcmp(arg, "-n") && i + 1 < argc)
    {
      num_packets = atoi(argv[++i]);
    }
    else if (!strcmp(arg, "-w") && i + 1 < argc)
    {
      num_workers = atoi(argv[++i]);
    }
    else
    {
      tmc_task_die("Unknown option '%s'.", arg);
    }
  }


  // Determine the available cpus.
  result = tmc_cpus_get_my_affinity(&cpus);
  VERIFY(result, "tmc_cpus_get_my_affinity()");

  if (tmc_cpus_count(&cpus) < num_workers)
    tmc_task_die("Insufficient cpus.");


  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_shared(&alloc);

  shared = tmc_alloc_map(&alloc, sizeof(*shared));

  context = &shared->context;

  // Get the instance.
  instance = gxio_mpipe_link_instance(link_name);
  if (instance < 0)
    tmc_task_die("Link '%s' does not exist.", link_name);

  // Start the driver.
  result = gxio_mpipe_init(context, instance);
  VERIFY(result, "gxio_mpipe_init()");

  gxio_mpipe_link_t link;
  result = gxio_mpipe_link_open(&link, context, link_name, 0);
  VERIFY(result, "gxio_mpipe_link_open()");

  int channel = gxio_mpipe_link_channel(&link);


  // Allocate some iqueues.
  shared->iqueues =
    tmc_alloc_map(&alloc, num_workers * sizeof(*shared->iqueues));
  if (shared->iqueues == NULL)
    tmc_task_die("Failure in 'calloc()'.");

  // Allocate some NotifRings.
  result = gxio_mpipe_alloc_notif_rings(context, num_workers, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_rings()");
  unsigned int ring = result;

  // Init the NotifRings.
  size_t notif_ring_entries = 512;
  size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);
  unsigned int needed = notif_ring_size + sizeof(gxio_mpipe_iqueue_t); 
  for (int i = 0; i < num_workers; i++)
  {
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_shared(&alloc);
    tmc_alloc_set_home(&alloc, tmc_cpus_find_nth_cpu(&cpus, i));
    // The ring must use physically contiguous memory, but the iqueue
    // can span pages, so we use "notif_ring_size", not "needed".
    tmc_alloc_set_pagesize(&alloc, notif_ring_size);
    void* iqueue_mem = tmc_alloc_map(&alloc, needed);
    if (iqueue_mem == NULL)
      tmc_task_die("Failure in 'tmc_alloc_map()'.");
    gxio_mpipe_iqueue_t* iqueue = iqueue_mem + notif_ring_size;
    result = gxio_mpipe_iqueue_init(iqueue, context, ring + i,
                                    iqueue_mem, notif_ring_size, 0);
    VERIFY(result, "gxio_mpipe_iqueue_init()");
    shared->iqueues[i] = iqueue;
  }


  // Allocate one huge page to hold the edma ring, the buffer stack,
  // and all the packets.  This should be more than enough space.
  size_t page_size = tmc_alloc_get_huge_pagesize();
  tmc_alloc_set_huge(&alloc);
  void* page = tmc_alloc_map(&alloc, page_size);
  assert(page);

  void* mem = page;


  // Allocate a NotifGroup.
  result = gxio_mpipe_alloc_notif_groups(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_groups()");
  int group = result;

  // Allocate some buckets.
  int num_buckets = 1024;
  result = gxio_mpipe_alloc_buckets(context, num_buckets, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  int bucket = result;

  // Init group and buckets, preserving packet order among flows.
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_ROUND_ROBIN;
  result = gxio_mpipe_init_notif_group_and_buckets(context, group,
                                                   ring, num_workers,
                                                   bucket, num_buckets, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");


  // Allocate an edma ring.
  result = gxio_mpipe_alloc_edma_rings(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_edma_rings");
  uint edma = result;

  // Init edma ring.
  size_t edma_ring_size = num_buffers * sizeof(gxio_mpipe_edesc_t);
  result = gxio_mpipe_equeue_init(&shared->equeue, context, edma, channel,
                                  mem, edma_ring_size, 0);
  VERIFY(result, "gxio_gxio_equeue_init()");
  mem += edma_ring_size;

  // Allocate a buffer stack.
  result = gxio_mpipe_alloc_buffer_stacks(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buffer_stacks()");
  int stack_idx = result;

  // Initialize the buffer stack.
  ALIGN(mem, 0x10000);
  size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(num_buffers);
  gxio_mpipe_buffer_size_enum_t buf_size = GXIO_MPIPE_BUFFER_SIZE_1664;
  result = gxio_mpipe_init_buffer_stack(context, stack_idx, buf_size,
                                        mem, stack_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  mem += stack_bytes;

  ALIGN(mem, 0x10000);

  // Register the entire huge page of memory which contains all the buffers.
  result = gxio_mpipe_register_page(context, stack_idx, page, page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");

  // Push some buffers onto the stack.
  for (int i = 0; i < num_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx, mem);
    mem += 1664;
  }

  // Paranoia.
  assert(mem <= page + page_size);


  // Register for packets.
  gxio_mpipe_rules_t rules;
  gxio_mpipe_rules_init(&rules, context);
  gxio_mpipe_rules_begin(&rules, bucket, num_buckets, NULL);
  result = gxio_mpipe_rules_commit(&rules);
  VERIFY(result, "gxio_mpipe_rules_commit()");


  tmc_sync_barrier_init(&shared->barrier, num_workers);


  // Go parallel.

  int watch_forked_children = tmc_task_watch_forked_children(1);

  int rank;
  for (rank = 1; rank < num_workers; rank++)
  {
    pid_t child = fork();
    if (child < 0)
      tmc_task_die("Failure in 'fork()'.");
    if (child == 0)
      goto done;
  }
  rank = 0;

  (void)tmc_task_watch_forked_children(watch_forked_children);

 done:

  main_aux(rank);

  // FIXME: Wait until pending egress is "done".
  for (int i = 0; i < 1000000; i++)
    __insn_mfspr(SPR_PASS);

  return 0;
}
