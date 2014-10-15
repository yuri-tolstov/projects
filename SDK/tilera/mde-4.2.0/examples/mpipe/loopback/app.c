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
#include <pthread.h>

#include <tmc/alloc.h>

#include <arch/atomic.h>
#include <arch/cycle.h>
#include <arch/sim.h>

#include <gxio/mpipe.h>

#include <tmc/cpus.h>
#include <tmc/mem.h>
#include <tmc/sync.h>
#include <tmc/task.h>


// Help synchronize thread creation.
static tmc_sync_barrier_t barrier;


// The number of packets to process.
static int num_packets = 1000;

// The number of workers to use.
static unsigned int num_workers = 1;

// The number of buffers and the edma ring size.
static unsigned int num_buffers = 2048;

// The initial affinity.
static cpu_set_t cpus;

// The mPIPE instance.
static int instance;

// The mpipe context (shared by all workers).
static gxio_mpipe_context_t context_body;
static gxio_mpipe_context_t* const context = &context_body;

// The buffer stack.
static gxio_mpipe_buffer_size_enum_t stack_kind = GXIO_MPIPE_BUFFER_SIZE_1664;
static int stack_idx;

// The ingress queues (one per worker).
static gxio_mpipe_iqueue_t** iqueues;

// The egress queue (shared by all workers).
static gxio_mpipe_equeue_t equeue_body;
static gxio_mpipe_equeue_t* const equeue = &equeue_body;

// The total number of packets forwarded by all workers.
static volatile unsigned long total = 0;



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



static void
dump_memory(unsigned char* p, size_t n)
{
  for (size_t j = 0; j < n; j++)
  {
    printf("0x%02x ", p[j]);
    if (((j + 1) & 15) == 0)
      putchar('\n');
  }
  putchar('\n');
}


static void
drain_packets(int rank, gxio_mpipe_iqueue_t* iqueue)
{
  gxio_mpipe_idesc_t drain;
  while (gxio_mpipe_iqueue_try_get(iqueue, &drain) == 0)
  {
    gxio_mpipe_iqueue_drop(iqueue, &drain);
    printf("Rank %d drained a packet.\n", rank);
  }
}



// The main function for each worker thread.
//
static void*
main_aux(void* arg)
{
  int result;

  int rank = (long)arg;

  gxio_mpipe_iqueue_t* iqueue = iqueues[rank];


  // Bind to a single cpu.
  result = tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank));
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  tmc_sync_barrier_wait(&barrier);


  // Paranoia.
  drain_packets(rank, iqueue);

  tmc_sync_barrier_wait(&barrier);


  // Egress some packets.

  for (int i = 0; i < 16; i++)
  {
    // ISSUE: Is there a minimum legal size?
    uint size = 0x70 + (0x10 * rank) + i;
    uint fill = size;

    void* buffer = gxio_mpipe_pop_buffer(context, stack_idx);
    if (buffer == NULL)
      tmc_task_die("Could not allocate initial buffer!");

    // NOTE: This affects the flow hash.
    memset(buffer, fill, size);

    __insn_mf();

    gxio_mpipe_edesc_t edesc = {{
        .bound = 1,
        .xfer_size = size,
        .va = (uintptr_t)buffer,
        .stack_idx = stack_idx,
        .inst = instance,
        .hwb = 1,
        .size = stack_kind,
      }};

    gxio_mpipe_equeue_put(equeue, edesc);
  }


  tmc_sync_barrier_wait(&barrier);


  if (rank == 0)
  {
    // HACK: Pause briefly, to let everyone finish passing the barrier.
    for (int i = 0; i < 10000; i++)
      __insn_mfspr(SPR_PASS);

    // Allow packets to flow.
    sim_enable_mpipe_links(instance, -1);
  }


  // Forward packets.

  uint64_t first_cycle = get_cycle_count();

  uint local = 0;

  while (total < num_packets)
  {
    gxio_mpipe_idesc_t* idesc;

    // Check for a packet.
    if (gxio_mpipe_iqueue_try_peek(iqueue, &idesc) > 0)
    {
      if (idesc->be)
        tmc_task_die("Insufficient buffers!");

      if (gxio_mpipe_iqueue_drop_if_bad(iqueue, idesc))
        goto done;

      unsigned char* va = gxio_mpipe_idesc_get_va(idesc);
      uint size = idesc->l2_size;

      unsigned int len = (size < 126) ? size : 126;
      unsigned char buf[128];
      memcpy(buf + 2, va, len);

      uint8_t fill = size;

      for (int k = 0; k < size; k++)
      {
        if (va[k] != fill)
        {
          printf("Corruption (0x%02x != 0x%02x) at byte 0x%x/0x%x:\n",
                 va[k], fill, k, size);
          dump_memory(va, size);
          printf("Snapshot:\n");
          dump_memory(buf + 2, len);
          tmc_task_die("Corrupt packet!");
        }
      }

      // TODO: Rewrite the packet header?

      // Prepare to forward.
      gxio_mpipe_edesc_t edesc;
      gxio_mpipe_edesc_copy_idesc(&edesc, idesc);

      gxio_mpipe_equeue_put(equeue, edesc);

    done:

      // We are done with the packet.
      gxio_mpipe_iqueue_consume(iqueue, idesc);

      // Count packets (atomically).
      arch_atomic_add(&total, 1);

      local++;
    }
  }

  printf("Rank %d handled %d packets in %lld cycles.\n",
         rank, local, get_cycle_count() - first_cycle);

  drain_packets(rank, iqueue);

  tmc_sync_barrier_wait(&barrier);

  return (void*)NULL;
}


int
main(int argc, char** argv)
{
  int result;

  char* link_name = "loop0";

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


  if (strncmp(link_name, "loop", 4) != 0)
    tmc_task_die("Must use a 'loop*' link.");

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
  iqueues = calloc(num_workers, sizeof(*iqueues));
  if (iqueues == NULL)
    tmc_task_die("Failure in 'calloc()'.");

  // Allocate some NotifRings.
  result = gxio_mpipe_alloc_notif_rings(context, num_workers, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_rings()");
  unsigned int ring = result;

  // Init the NotifRings.
  size_t notif_ring_entries = 512;
  size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);
  size_t needed = notif_ring_size + sizeof(gxio_mpipe_iqueue_t);
  for (int i = 0; i < num_workers; i++)
  {
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
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
    iqueues[i] = iqueue;
  }


  // Allocate one huge page to hold the edma ring, the buffer stack,
  // and all the buffers.  This should be more than enough space.
  size_t page_size = tmc_alloc_get_huge_pagesize();
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* page = tmc_alloc_map(&alloc, page_size);
  assert(page != NULL);
  void* mem = page;


  // Allocate a NotifGroup.
  result = gxio_mpipe_alloc_notif_groups(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_groups()");
  int group = result;

#if 1
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_ROUND_ROBIN;
  int num_buckets = 1;
#else
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
  int num_buckets = 128;
#endif

  // Allocate some buckets. The default mPipe classifier requires
  // the number of buckets to be a power of two (maximum of 4096).
  result = gxio_mpipe_alloc_buckets(context, num_buckets, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  int bucket = result;

  // Init group and bucket.
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
  result = gxio_mpipe_equeue_init(equeue, context, edma, channel,
                                  mem, edma_ring_size, 0);
  VERIFY(result, "gxio_gxio_equeue_init()");
  mem += edma_ring_size;

  // Allocate a buffer stack.
  result = gxio_mpipe_alloc_buffer_stacks(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buffer_stacks()");
  stack_idx = result;

  // Initialize the buffer stack.  Must be aligned mod 64K.
  ALIGN(mem, 0x10000);
  size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(num_buffers);
  result = gxio_mpipe_init_buffer_stack(context, stack_idx, stack_kind,
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


  tmc_sync_barrier_init(&barrier, num_workers);

  if (num_workers > 1)
  {
    pthread_t threads[num_workers];
    for (int i = 0; i < num_workers; i++)
    {
      if (pthread_create(&threads[i], NULL, main_aux, (void*)(intptr_t)i) != 0)
        tmc_task_die("Failure in 'pthread_create()'.");
    }
    for (int i = 0; i < num_workers; i++)
    {
      if (pthread_join(threads[i], NULL) != 0)
        tmc_task_die("Failure in 'pthread_join()'.");
    }
  }
  else
  {
    (void)main_aux((void*)(intptr_t)0);
  }

  return 0;
}
