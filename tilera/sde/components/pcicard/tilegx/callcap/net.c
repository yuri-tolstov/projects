/******************************************************************************/
/* File:   callcap/net.c                                                      */
/******************************************************************************/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/dataplane.h>

#include <tmc/alloc.h>

#include <arch/atomic.h>
#include <arch/sim.h>

#include <gxio/mpipe.h>

#include <tmc/cpus.h>
#include <tmc/mem.h>
#include <tmc/spin.h>
#include <tmc/sync.h>
#include <tmc/task.h>

#include "local.h"


// Align "p" mod "align", assuming "p" is a "void*".
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)


// Maximum "batch" size.
#define BATCH 16


// Help synchronize thread creation.
static tmc_sync_barrier_t sync_barrier;
static tmc_spin_barrier_t spin_barrier;

// True if "--flip" was specified.
static bool flip;

// True if "--jumbo" was specified.
static bool jumbo;

// The number of packets to process.
static int num_packets = 1000;

// The flag to indicate packet forward is done.
static volatile bool done = false;

// The number of workers to use.
#ifdef NUM_WORKERS
static unsigned int num_workers = NUM_WORKERS;
#else
static unsigned int num_workers = 1;
#endif

// The number of entries in the equeue ring - 2K (2048).
static unsigned int equeue_entries = GXIO_MPIPE_EQUEUE_ENTRY_2K;

// The initial affinity.
static cpu_set_t cpus;

// The mPIPE instance.
static int instance;

// The mpipe context (shared by all workers).
static gxio_mpipe_context_t context_body;
static gxio_mpipe_context_t* const context = &context_body;

// The ingress queues (one per worker).
static gxio_mpipe_iqueue_t** iqueues;

// The egress queue (shared by all workers).
static gxio_mpipe_equeue_t equeue_body;
static gxio_mpipe_equeue_t* const equeue = &equeue_body;

// The total number of packets forwarded by all workers.
// Reserve a cacheline for "total" to eliminate the false sharing.
#define total total64.v
struct {
  volatile unsigned long v __attribute__ ((aligned(CHIP_L2_LINE_SIZE())));
} total64 = { 0 };

#if 0
// Allocate memory for a buffer stack and its buffers, initialize the
// stack, and push buffers onto it.
//
static void
create_stack(gxio_mpipe_context_t* context, int stack_idx,
             gxio_mpipe_buffer_size_enum_t buf_size, int num_buffers)
{
  int result;

  // Extract the actual buffer size from the enum.
  size_t size = gxio_mpipe_buffer_size_enum_to_buffer_size(buf_size);

  // Compute the total bytes needed for the stack itself.
  size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(num_buffers);

  // Round up so that the buffers will be properly aligned.
  stack_bytes += -(long)stack_bytes & (128 - 1);

  // Compute the total bytes needed for the stack plus the buffers.
  size_t needed = stack_bytes + num_buffers * size;

  // Allocate up to 16 pages of the smallest suitable pagesize.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_pagesize(&alloc, needed / 16);
  size_t pagesize = tmc_alloc_get_pagesize(&alloc);
  int pages = (needed + pagesize - 1) / pagesize;
  void* mem = tmc_alloc_map(&alloc, pages * pagesize);
  if (mem == NULL)
    tmc_task_die("Could not allocate buffer pages.");

  // Initialize the buffer stack.
  result = gxio_mpipe_init_buffer_stack(context, stack_idx, buf_size,
                                        mem, stack_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");

  // Register the buffer pages.
  for (int i = 0; i < pages; i++)
  {
    result = gxio_mpipe_register_page(context, stack_idx,
                                      mem + i * pagesize, pagesize, 0);
    VERIFY(result, "gxio_mpipe_register_page()");
  }

  // Push the actual buffers.
  mem += stack_bytes;
  for (int i = 0; i < num_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx, mem);
    mem += size;
  }
}
#endif


/******************************************************************************/
/* Thread:   net_thread                                                       */
/******************************************************************************/
void* net_thread(void* arg)
{
  int c, i; /*Return code, Index*/
  int ifx = (long)arg; /*Interface index*/
  gxio_mpipe_iqueue_t* iqueue = iqueues[ifx]; /*Ingress queue*/

  /*Bind to a single cpu.*/
   if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, ifx)) < 0) {
      tmc_task_die("Failed to setup CPU affinity\n");
   }
   mlockall(MCL_CURRENT);
   tmc_sync_barrier_wait(&syncbar);
   tmc_spin_barrier_wait(&spinbar);

   if (set_dataplane(DP_DEBUG) < 0) {
      tmc_task_die("Failed to setup dataplane\n");
   }
   tmc_spin_barrier_wait(&spinbar);

   if (ifx == 0) {
      /*Pause briefly, to let everyone finish passing the barrier.*/
      for (i = 0; i < 10000; i++) __insn_mfspr(SPR_PASS);
      /*Allow packets to flow.*/
      sim_enable_mpipe_links(mpipei, -1);
   }

  // Local version of "total". The optimization is to save a global variable
  // load (most likely in L3) per iteration and share invalidation.
  unsigned long local_total = 0;

  while (local_total < num_packets || num_packets < 0)
  {
    // Wait for packets to arrive.

    gxio_mpipe_idesc_t* idescs;

    int n = gxio_mpipe_iqueue_try_peek(iqueue, &idescs);
    if (n > BATCH)
      n = BATCH;
    else if (n <= 0)
    {
      if (done)
        goto  L_done;
      continue;
    }
    // Prefetch the actual idescs.
    tmc_mem_prefetch(idescs, n * sizeof(*idescs));

    gxio_mpipe_edesc_t edescs[BATCH];

    for (int i = 0; i < n; i++)
    {
      gxio_mpipe_idesc_t* idesc = &idescs[i];
      gxio_mpipe_edesc_t* edesc = &edescs[i];

      // Prepare to forward (or drop).
      gxio_mpipe_edesc_copy_idesc(edesc, idesc);

      // Drop "error" packets (but ignore "checksum" problems).
      if (idesc->be || idesc->me || idesc->tr || idesc->ce)
        edesc->ns = 1;
    }

    if (flip)
    {
      for (int i = 0; i < n; i++)
      {
        if (!edescs[i].ns)
        {
          void* start = gxio_mpipe_idesc_get_l2_start(&idescs[i]);
          size_t length = gxio_mpipe_idesc_get_l2_length(&idescs[i]);

          // Prefetch entire packet (12 bytes would be sufficient).
          tmc_mem_prefetch(start, length);
        }
      }

      for (int i = 0; i < n; i++)
      {
        if (!edescs[i].ns)
        {
          void* start = gxio_mpipe_idesc_get_l2_start(&idescs[i]);

          // Flip source/dest mac addresses.
          unsigned char tmac[6];
          void* dmac = start + 0;
          void* smac = start + 6;
          memcpy(tmac, dmac, 6);
          memcpy(dmac, smac, 6);
          memcpy(smac, tmac, 6);
        }
      }

      // Flush.
      __insn_mf();
    }

    // Reserve slots.  NOTE: This might spin.
    long slot = gxio_mpipe_equeue_reserve_fast(equeue, n);

    // Egress the packets.
    for (int i = 0; i < n; i++)
      gxio_mpipe_equeue_put_at(equeue, edescs[i], slot + i);

    // Consume the packets.
    for (int i = 0; i < n; i++)
      gxio_mpipe_iqueue_consume(iqueue, &idescs[i]);

    // Count packets (atomically). Let the local_total be the new total
    // value after the atomic add. Note: there is no explicit load to "total".
    local_total = arch_atomic_add(&total, n) + n;
  }

  // Forwarding is done!
  done = true;

  // Make done visible to all workers.
  __insn_mf();

 L_done:

  return (void *)NULL;
}


