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

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>
#include <sched.h>

#include <tmc/alloc.h>

#include <arch/atomic.h>
#include <arch/cycle.h>
#include <arch/sim.h>

#include <gxio/mpipe.h>

#include <tmc/cpus.h>
#include <tmc/mem.h>
#include <tmc/sync.h>
#include <tmc/task.h>


// Define this to verify packet contents.
#define PARANOIA


// FIXME: Define these somewhere!
#define likely(cond) __builtin_expect((cond), 1)
#define unlikely(cond) __builtin_expect((cond), 0)


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



// Help synchronize thread creation.
static tmc_sync_barrier_t barrier;


// The number of packets to process.
static int num_packets = 1000;

// The number of workers to use.
static unsigned int num_workers = 1;

// The headroom.
static unsigned int headroom = 2;

// The (loopback) channel.
static int channel = 16;

// The edma ring size.
static unsigned int equeue_entries = 2048;

// The initial affinity.
static cpu_set_t cpus;

// The mPIPE instance.
static int instance;

// The mpipe context (shared by all workers).
static gxio_mpipe_context_t context_body;
static gxio_mpipe_context_t* const context = &context_body;

// Number of seed packets per worker.
static unsigned int seed_per_worker = 64;

// The "large" buffer stack.
static unsigned int large_buffers;
static gxio_mpipe_buffer_size_enum_t large_size = 1664;
static gxio_mpipe_buffer_size_enum_t large_kind = GXIO_MPIPE_BUFFER_SIZE_1664;
static int large_idx;

// The "small" buffer stack.
static unsigned int small_buffers = 2048;
static gxio_mpipe_buffer_size_enum_t small_size = 128;
static gxio_mpipe_buffer_size_enum_t small_kind = GXIO_MPIPE_BUFFER_SIZE_128;
static int small_idx;

// The ingress queues (one per worker).
static gxio_mpipe_iqueue_t** iqueues;

// The egress queue (shared by all workers).
static gxio_mpipe_equeue_t equeue_body;
static gxio_mpipe_equeue_t* const equeue = &equeue_body;

// The total number of packets forwarded by all workers.
static volatile unsigned long total = 0;

// Used to salt the "fill" values.
static unsigned int base = 0x00;


#ifdef PARANOIA

extern void
spew(const char* format, ...)
  __attribute__((format(printf, 1, 2)));

void
spew(const char* format, ...)
{
  int cpu = sched_getcpu();

  char buf[4096];

  sprintf(buf, "[%02d,%02d] ", cpu, channel);

  va_list args;
  va_start(args, format);
  vsprintf(buf + strlen(buf), format, args);
  va_end(args);

  sprintf(buf + strlen(buf), "\n");

  if (sim_is_simulator())
    sim_print(buf);
  else
    fprintf(stderr, "%s", buf);
}


static void
dump_memory(unsigned char* p, size_t n)
{
  unsigned long i;
  char buf[128];

  spew("Dumping %zd bytes at %p", n, p);

  for (i = 0; i < n; i++)
  {
    if ((i & 0xf) == 0)
      sprintf(buf, "%8.8lx:", i);
    sprintf(buf + strlen(buf), " %2.2x", p[i]);
    if ((i & 0xf) == 0xf || i == n - 1)
      spew("%s", buf);
  }
}


static void
bad_packet(gxio_mpipe_idesc_t* idesc, const char* reason)
{
  unsigned char* va = gxio_mpipe_idesc_get_va(idesc);
  unsigned int size = idesc->l2_size;

  unsigned int sqn = idesc->packet_sqn;

  spew("Packet 0x%x (va = %p, size = %d) is bad: %s",
       sqn, va, size, reason);

  spew("Channel %d, bucket %d, stack %d, ring = %d, dest = %d",
       idesc->channel, idesc->bucket_id, idesc->stack_idx, idesc->notif_ring,
       idesc->dest);

  spew("Cd0 = 0x%x, Cd1 = 0x%llx, Cd2 = 0x%llx, Cd3 = 0x%llx",
       idesc->custom0, idesc->custom1, idesc->custom2, idesc->custom3);

  spew("Timestamp = %d NS %d CS=%d (0x%x)",
       idesc->time_stamp_sec, idesc->time_stamp_ns,
       idesc->cs, idesc->csum_seed_val);

  // NOTE: Assume chained.
  gxio_mpipe_edesc_t chain;
  chain.words[1] = idesc->words[7];
  while (chain.c == MPIPE_EDMA_DESC_WORD1__C_VAL_CHAINED)
  {
    va = (void*)(intptr_t)(chain.va & ~0x7f);
    spew("Chain va = %p", va);
    dump_memory(va, small_size);
    chain.words[1] = *(uint64_t*)va;
  }

  tmc_task_die("Bad packet: %s", reason);
}


static void verify_packet(gxio_mpipe_idesc_t* idesc)
{
  assert(!idesc->me);
  assert(!idesc->tr);
  assert(!idesc->ce);
  assert(!idesc->ct);
  assert(!idesc->nr);
  assert(!idesc->be);

  //--assert(idesc->sq);
  assert(idesc->ts);
  assert(idesc->ps);

  if (idesc->channel != channel)
    bad_packet(idesc, "bad channel");

  // HACK: Assume chained.
  assert(idesc->c == MPIPE_EDMA_DESC_WORD1__C_VAL_CHAINED);

  int offset = idesc->va & 0x7f;
  if (offset != headroom + 8)
    bad_packet(idesc, "Bad offset");
}

#endif


// The main loop for each worker thread.
//
// Return the number of packets processed.
//
static unsigned long __attribute__((noinline))
main_loop(gxio_mpipe_iqueue_t* iqueue)
{
  unsigned long local = 0;

  while (total < num_packets)
  {
    gxio_mpipe_idesc_t* idesc;

    // Check for a packet.
    int n = gxio_mpipe_iqueue_try_peek(iqueue, &idesc);
    if (unlikely(n <= 0))
      continue;

    // ISSUE: What is the best batch size?
    if (n > 32) n = 32;

    // Prefetch the idescs (64 bytes each).
    for (int i = 0; i < n; i++)
    {
      __insn_prefetch(&idesc[i]);
    }

    // Check for "bad" packets.
    for (int i = 0; i < n; i++)
    {
      // HACK: Technically we should just "skip" bad packets,
      // but that would complicate this particular example.
      if (gxio_mpipe_iqueue_drop_if_bad(iqueue, &idesc[i]))
      {
        if (idesc[i].be)
          tmc_task_die("Insufficient buffers!");
        else
          tmc_task_die("Bad packet!");
      }
    }

    // Prefetch the packet data (up to 64 bytes of each packet).
    for (int i = 0; i < n; i++)
    {
      unsigned char* va = (void*)(intptr_t)idesc[i].va;
      __insn_prefetch(va);
    }

    // Modify the packet data.
    for (int i = 0; i < n; i++)
    {
      unsigned char* va = (void*)(intptr_t)idesc[i].va;

      // HACK: Swap the first two bytes.  Since both bytes are
      // definitely in the first buffer, we can ignore chaining.
      unsigned char tmp = va[0];
      va[0] = va[1];
      va[1] = tmp;
    }

    // Reserve some egress slots.
    int64_t slot = gxio_mpipe_equeue_reserve_fast(equeue, n);
    if (slot < 0)
      tmc_task_die("Cannot reserve queue slots!");

    // Flush the modifications.
    // We do this after reserving slots, for efficiency.
    __insn_mf();

    // Forward the packets.
    for (int i = 0; i < n; i++, idesc++)
    {

#ifdef PARANOIA
      verify_packet(idesc);

#if 1
      unsigned int size = idesc->l2_size;

      uint8_t fill = (size & 0xf) + base;

      uint k = 0;

      gxio_mpipe_edesc_t chain;
      chain.words[1] = idesc->words[7];
      while (chain.c == MPIPE_EDMA_DESC_WORD1__C_VAL_CHAINED)
      {
        // The address of the first packet byte in this buffer.
        unsigned char* va = (void*)(intptr_t)(chain.va);

        // This will be "8 + headroom" for the first buffer, and "8"
        // for subsequent buffers.
        uint offset = chain.va & 0x7f;

        // The maximum number of available bytes in this buffer.
        unsigned int avail = small_size - offset;

        // The actual number of available bytes in this buffer.
        unsigned int limit = ((size - k) < avail) ? (size - k) : avail;

        for (int j = 0; j < limit; j++, k++)
        {
          if (va[j] != fill)
          {
            spew("Packet %ld byte 0x%02x should be 0x%02x at %d/%d:",
                 local + i, va[j], fill, k, size);
            bad_packet(idesc, "corrupt");
          }
        }

        chain.words[1] = *(uint64_t*)(va - offset);
      }
#endif
#endif

      // Forward the packet.

      // Prepare to forward.
      gxio_mpipe_edesc_t edesc;
      gxio_mpipe_edesc_copy_idesc(&edesc, idesc);

      gxio_mpipe_equeue_put_at(equeue, edesc, slot + i);

      // Consume the packet.
      gxio_mpipe_iqueue_consume(iqueue, idesc);
    }

    local += n;

    // Count packets (atomically).
    arch_atomic_add(&total, n);
  }

  return local;
}


// The main function for each worker thread.
//
static void*
main_func(void* arg)
{
  int result;

  int rank = (long)arg;

  gxio_mpipe_iqueue_t* iqueue = iqueues[rank];


  // Bind to a single cpu.
  int cpu = tmc_cpus_find_nth_cpu(&cpus, rank);
  result = tmc_cpus_set_my_cpu(cpu);
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  tmc_sync_barrier_wait(&barrier);


  // Egress some packets.

  // NOTE: We assume that "size + headroom > small_size", such that
  // all incoming packets will be "chained".

  for (int i = 0; i < seed_per_worker; i++)
  {
    unsigned int size = 0x400 + ((base + (0x10 * rank) + i) & 0xff);

    unsigned int fill = (size & 0xf) + base;

    void* buffer = gxio_mpipe_pop_buffer(context, large_idx);
    if (buffer == NULL)
      tmc_task_die("Could not pop seed buffer!");

    memset(buffer, fill, size);
    __insn_mf();

    gxio_mpipe_edesc_t edesc = {{
        .bound = 1,
        .xfer_size = size,
        .va = (uintptr_t)buffer,
        .stack_idx = large_idx,
        .inst = instance,
        .hwb = 1,
        .size = large_kind,
      }};

    gxio_mpipe_equeue_put(equeue, edesc);
  }


  tmc_sync_barrier_wait(&barrier);


  if (rank == 0 && sim_is_simulator())
  {
    // HACK: Pause briefly, to let everyone finish passing the barrier.
    for (int i = 0; i < 10000; i++)
      __insn_mfspr(SPR_PASS);

    // Allow packets to flow.
    sim_enable_mpipe_links(instance, -1);
  }


  // Forward packets.

  uint64_t start = get_cycle_count();

  unsigned long local = main_loop(iqueue);

  printf("Rank %d handled %ld packets in %lld cycles.\n",
         rank, local, get_cycle_count() - start);

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
    else if (!strcmp(arg, "--base") && i + 1 < argc)
    {
      base = strtol(argv[++i], NULL, 0);
    }
    else if (!strcmp(arg, "--headroom") && i + 1 < argc)
    {
      headroom = atoi(argv[++i]);
    }
    else if (!strcmp(arg, "--link") && i + 1 < argc)
    {
      link_name = argv[++i];
    }
    else
    {
      tmc_task_die("Unknown option '%s'.", arg);
    }
  }

  // Update "large_buffers".
  large_buffers = num_workers * seed_per_worker;


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
    int cpu = tmc_cpus_find_nth_cpu(&cpus, i);
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_home(&alloc, cpu);
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
  //--tmc_alloc_set_home(&alloc, 0);
  void* page = tmc_alloc_map(&alloc, page_size);
  assert(page != NULL);
  void* mem = page;

  // Allocate a NotifGroup.
  result = gxio_mpipe_alloc_notif_groups(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_groups()");
  int group = result;

  // Allocate some buckets. The default mPipe classifier requires
  // the number of buckets to be a power of two (maximum of 4096).
  int num_buckets = 1024;
  result = gxio_mpipe_alloc_buckets(context, num_buckets, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  int bucket = result;

  // Init group and bucket.
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_ROUND_ROBIN;
  result = gxio_mpipe_init_notif_group_and_buckets(context, group,
                                                   ring, num_workers,
                                                   bucket, num_buckets, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");


  // Allocate an edma ring.
  result = gxio_mpipe_alloc_edma_rings(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_edma_rings");
  unsigned int edma = result;

  // Init edma ring.
  size_t edma_ring_size = equeue_entries * sizeof(gxio_mpipe_edesc_t);
  result = gxio_mpipe_equeue_init(equeue, context, edma, channel,
                                  mem, edma_ring_size, 0);
  VERIFY(result, "gxio_gxio_equeue_init()");
  mem += edma_ring_size;


  // Allocate two buffer stacks, one for "large" packets and one for
  // "small" packets.  The former is used only for egressing the
  // initial packets, to save us from having to hand-construct chained
  // packets.  The latter is used for ingressing chained packets, and
  // forwarding them.
  result = gxio_mpipe_alloc_buffer_stacks(context, 2, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buffer_stacks()");
  large_idx = result;
  small_idx = result + 1;

  // Initialize the "large" buffer stack.  Must be aligned mod 64K.
  ALIGN(mem, 0x10000);
  size_t large_bytes = gxio_mpipe_calc_buffer_stack_bytes(large_buffers);
  result = gxio_mpipe_init_buffer_stack(context, large_idx, large_kind,
                                        mem, large_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  mem += large_bytes;

  // Initialize the "small" buffer stack.  Must be aligned mod 64K.
  ALIGN(mem, 0x10000);
  size_t small_bytes = gxio_mpipe_calc_buffer_stack_bytes(small_buffers);
  result = gxio_mpipe_init_buffer_stack(context, small_idx, small_kind,
                                        mem, small_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  mem += small_bytes;

  ALIGN(mem, 0x10000);

  // Register the entire huge page of memory which contains the large buffers.
  result = gxio_mpipe_register_page(context, large_idx, page, page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");

  // Populate the large buffer stack.
  for (int i = 0; i < large_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, large_idx, mem);
    mem += large_size;
  }

  // Register the entire huge page of memory which contains the small buffers.
  result = gxio_mpipe_register_page(context, small_idx, page, page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");

  // Populate the small buffer stack.
  for (int i = 0; i < small_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, small_idx, mem);
    mem += small_size;
  }

  // Paranoia.
  assert(mem <= page + page_size);

  // Register for packets.  Note that we use the "small" buffer stack
  // for packets of all sizes, using the fact that "-1" means to use
  // the next value in the array (transitively).
  gxio_mpipe_rules_t rules;
  gxio_mpipe_rules_init(&rules, context);
  gxio_mpipe_rules_stacks_t stacks =
    { { -1, -1, -1, -1, -1, -1, -1, small_idx } };
  gxio_mpipe_rules_begin(&rules, bucket, num_buckets, &stacks);
  gxio_mpipe_rules_set_headroom(&rules, headroom);
  gxio_mpipe_rules_add_channel(&rules, channel);
  result = gxio_mpipe_rules_commit(&rules);
  VERIFY(result, "gxio_mpipe_rules_commit()");


  tmc_sync_barrier_init(&barrier, num_workers);

  if (num_workers > 1)
  {
    pthread_t threads[num_workers];
    for (int i = 0; i < num_workers; i++)
    {
      if (pthread_create(&threads[i], NULL, main_func,
                         (void*)(intptr_t)i) != 0)
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
    (void)main_func((void*)(intptr_t)0);
  }

  return 0;
}
