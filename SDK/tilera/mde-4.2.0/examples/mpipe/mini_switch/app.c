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

// Takes traffic from the world on one link, and forwards it to the
// local linux, and/or up to three other processors, and vice-versa,
// based on dmacs.
//
// Uses a special linux hack.


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

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


// Align "p" mod "align", assuming "p" is a "void*".
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)

#define MINISWITCH_DEBUG 0
#define MINISWITCH_MEASURE_LATENCY 0
#define MINISWITCH_USE_PREFETCH 1

// Help synchronize thread creation.
static tmc_sync_barrier_t sync_barrier;
static tmc_spin_barrier_t spin_barrier;


// The initial affinity.
static cpu_set_t cpus;

// The mPIPE instance.
static int instance;

// The number of dmacs to support.
static unsigned int num_links;

// The link names.  The extra two are "loop0" and "loop1".
static const char* link_names[4 + 2];

// The dmacs for each link.
static gxio_mpipe_rules_dmac_t dmacs[4];

// The channels.  The extra ones are "loop0" and "loop1".
static int channels[4 + 2];

// The number of entries in the equeue ring.
static unsigned int equeue_entries = 2048;

// The mpipe context (shared by all workers).
static gxio_mpipe_context_t context_body;
static gxio_mpipe_context_t* const context = &context_body;

// The ingress queues.  One for each dmac plus a "catchall" ring.
static gxio_mpipe_iqueue_t* iqueues[4 + 1];

// The egress queues.  One for each dmac plus an "uplink" ring.
// Note that the first one uses "loop0" to send to the local linux.
static gxio_mpipe_equeue_t* equeues;


// Help check for errors.
#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)


// The main function for each worker thread.
//
// Rank 0 forwards to linux on loop0.
// Rank num_links handles multicast, broadcast, and random traffic.
// The others simply forward to the corresponding equeue.
//
static void*
main_aux(void* arg)
{
  int result;

  int rank = (long)arg;

#if MINISWITCH_MEASURE_LATENCY
  uint64_t rcv_cycle = 0;
  int n_pkts = 0;
#endif

  // Bind to a single cpu.
  result = tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, rank));
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  //--mlockall(MCL_CURRENT);

  tmc_sync_barrier_wait(&sync_barrier);

  //--result = set_dataplane(DP_DEBUG);
  //--VERIFY(result, "set_dataplane()");

  tmc_spin_barrier_wait(&spin_barrier);

  if (rank != num_links)
  {
    // Packets for a particular dmac get forwarded to the appropriate
    // equeue, including rank 0 forwarding packets to the local linux.

    gxio_mpipe_iqueue_t* iqueue = iqueues[rank];
    gxio_mpipe_equeue_t* equeue = &equeues[rank];

    while (1)
    {
      gxio_mpipe_idesc_t* idesc;

#if MINISWITCH_MEASURE_LATENCY
      uint64_t prev_done_cycle = __insn_mfspr(SPR_CYCLE);
      if (rcv_cycle)
      {
        printf("Rank %d: processed %d packets in %ld cycles\n", 
               rank, n_pkts, prev_done_cycle - rcv_cycle);
      }
#endif
      
      // Wait for packets to arrive.
      int n = gxio_mpipe_iqueue_peek(iqueue, &idesc);

#if MINISWITCH_MEASURE_LATENCY
      rcv_cycle = __insn_mfspr(SPR_CYCLE);
      n_pkts = n;
#endif

      if (n > 32)
        n = 32;
      
#if MINISWITCH_USE_PREFETCH
      // Prefetch idescs to get required fields from L3 to L1.
      // Note that "gxio_mpipe_iqueue_peek()" prefetches the first idesc.
      for (int i = 1; i < n; i++)
      {
        __insn_prefetch((void *)idesc + (i << 6));
      }
#endif

#if MINISWITCH_DEBUG > 0
      if (n > 1)
        printf("Rank %d peeked multiple packets: %d\n", rank, n);
#endif
        
#if MINISWITCH_DEBUG > 1
      printf("Rank %d peeked %d packets\n", rank, n);
      for (int i = 0; i < n; i++)
      {
        uint8_t* b = gxio_mpipe_idesc_get_va(&idesc[i]);
        printf("Dmac = %02x:%02x:%02x:%02x:%02x:%02x'.\n",
               b[0], b[1], b[2], b[3], b[4], b[5]);
      }
#endif

      // Prereserve slots.
      long slot = gxio_mpipe_equeue_reserve_fast(equeue, n);

      for (int i = 0; i < n; i++, idesc++)
      {
        gxio_mpipe_edesc_t edesc;
        gxio_mpipe_edesc_copy_idesc(&edesc, idesc);

        gxio_mpipe_equeue_put_at(equeue, edesc, slot + i);

        // We are done with the packet.
        gxio_mpipe_iqueue_consume(iqueue, idesc);
      }
    }
  }

  else
  {
    // HACK: Pause briefly, to let everyone finish passing the barrier.
    for (int i = 0; i < 10000; i++)
      __insn_mfspr(SPR_PASS);

    // Allow packets to flow.
    sim_enable_mpipe_links(instance, -1);

    // Forward packets.

    gxio_mpipe_iqueue_t* iqueue = iqueues[rank];
    gxio_mpipe_equeue_t* equeue = &equeues[rank];

    while (1)
    {
      // Wait for packets to arrive.

      gxio_mpipe_idesc_t* idesc;

#if MINISWITCH_MEASURE_LATENCY
      uint64_t prev_done_cycle = __insn_mfspr(SPR_CYCLE);
      if (rcv_cycle)
      {
        printf("Rank %d: processed %d packets in %ld cycles\n", 
               rank, n_pkts, prev_done_cycle - rcv_cycle);
      }
#endif

      int n = gxio_mpipe_iqueue_peek(iqueue, &idesc);

#if MINISWITCH_MEASURE_LATENCY
      rcv_cycle = __insn_mfspr(SPR_CYCLE);
      n_pkts = n;
#endif

      if (n > 32)
        n = 32;

      // ISSUE: Handle "idesc->be" properly!

#if MINISWITCH_USE_PREFETCH
      // Prefetch to get required idesc fields from L3 to L1.
      for (int i = 1; i < n; i++)
      {
        __insn_prefetch((void *)idesc + (i << 6));
      }
      
      // Prefetch to get 1st line of packet data into L1 so 
      // we can see if it's multicast.
      // These prefetches are dependant on the ones above so the
      // first couple may stall
      for (int i = 1; i < n; i++)
      {
        __insn_prefetch(gxio_mpipe_idesc_get_va(idesc + i));
      }
#endif
      
#if MINISWITCH_DEBUG > 0
      if (n > 1)
        printf("Fallback peeked multiple packets: %d\n", n);
#endif

#if MINISWITCH_DEBUG > 1
      printf("Fallback peeked %d packets: ", n);
      for (int i = 0; i < n; i++)
      {
        uint8_t* b = gxio_mpipe_idesc_get_va(&idesc[i]);
        printf("  channel %d bucket %d"
               " dmac = %02x:%02x:%02x:%02x:%02x:%02x.\n",
               idesc[i].channel, idesc[i].bucket_id,
               b[0], b[1], b[2], b[3], b[4], b[5]);
      }
#endif

      // Prereserve slots for uplink.
      long slot = gxio_mpipe_equeue_reserve_fast(equeue, n);

      for (int s = 0; s < n; s++, idesc++)
      {
        if (idesc->channel == channels[0])
        {
          // This came from the uplink and did not target one of the known
          // DMACs.  So send it to everyone (except uplink).
          // Uplink is equeues[num_links].          
          gxio_mpipe_edesc_t edesc;
          gxio_mpipe_edesc_copy_idesc(&edesc, idesc);

          void* bufs[num_links];
          for (int i = 0; i < num_links; i++)
          {
            void* buf = gxio_mpipe_pop_buffer(context, idesc->stack_idx);

            // FIXME: Handle this properly.
            assert(buf);

            memcpy(buf, gxio_mpipe_idesc_get_va(idesc), idesc->l2_size);
            
            bufs[i] = buf;
          }

          // Use MF to make sure memcpy's have completed before we
          // send the packets.         
          __insn_mf();

          for (int i = 0; i < num_links; i++)
          {
            edesc.va = (uintptr_t)bufs[i];
#if MINISWITCH_DEBUG > 1
            printf("   Broadcasting to link endpoint %d\n", i);
#endif
            gxio_mpipe_equeue_put(&equeues[i], edesc);
          }

          // We pre-reserved a slot in the uplink ring.  Use it to
          // return the original buffer.
          edesc.va = (uintptr_t)gxio_mpipe_idesc_get_va(idesc);
          edesc.ns = 1;
          // Return buffer via no-send to uplink since we reserved a slot.
          gxio_mpipe_equeue_put_at(equeue, edesc, slot + s);
        }
        else 
        {
          // This came from one of the endpoints.  
          // Forward to uplink and potentially broadcast.
          gxio_mpipe_edesc_t edesc;
          gxio_mpipe_edesc_copy_idesc(&edesc, idesc);

          // FIXME: Teach classifier to detect "broadcast/multicast".
          uint8_t* b = gxio_mpipe_idesc_get_va(idesc);
          if (b[0] & 1)
          {
            void* bufs[num_links];
            int skip_link = -1;
            // This is a broadcast - send to everyone except the ingress port.
            for (int i = 0; i < num_links; i++)
            {
              // Don't send on the link it came from.              
              int links_channel =
                channels[(i == 0) ? (num_links + 0) : (i % num_links)];
              if (idesc->channel == links_channel)
              { 
                skip_link = i;
                continue;
              }

              void* buf = gxio_mpipe_pop_buffer(context, idesc->stack_idx);

              // FIXME: Handle this properly.
              assert(buf);

              memcpy(buf, gxio_mpipe_idesc_get_va(idesc), idesc->l2_size);
              
              bufs[i] = buf;
            }
            
            // Use MF to make sure memcpy's have completed before we
            // send the packets.
            __insn_mf();

            for (int i = 0; i < num_links; i++)
            {
              // Don't send on the link it came from.
              if (i == skip_link)
                continue;

              edesc.va = (uintptr_t)bufs[i];

#if MINISWITCH_DEBUG > 1
              printf("   Broadcasting to link endpoint %d\n", i);
#endif
              gxio_mpipe_equeue_put(&equeues[i], edesc);
            }
          }
#if MINISWITCH_DEBUG > 1
          printf("   Forwarding to uplink\n");
#endif
          edesc.va = (uintptr_t)gxio_mpipe_idesc_get_va(idesc);
          
          gxio_mpipe_equeue_put_at(equeue, edesc, slot + s);
        }
        
        // We are done with the packet.
        gxio_mpipe_iqueue_consume(iqueue, idesc);
      }
    }
  }

  return (void*)NULL;
}


int
main(int argc, char** argv)
{
  int result;

  // Parse args.
  int i;
  for (i = 1; i < argc; i++)
  {
    char* arg = argv[i];

    if (arg[0] == '-')
    {
      tmc_task_die("Unknown option '%s'.", arg);
    }
    else
    {
      break;
    }
  }

  while (i < argc)
  {
    if (num_links >= 4)
      tmc_task_die("Too many links!");

    if (i + 1 >= argc)
      tmc_task_die("Usage: link dmac [link dmac ...]");

    // Save the link name.
    link_names[num_links] = argv[i];

    char* arg = argv[i + 1];
    for (int k = 0; k < 6; k++)
    {
      if (!isxdigit(arg[k*3]) || !isxdigit(arg[k*3+1]) || 
          arg[k*3+2] != ((k < 5) ? ':' : '\0'))
        tmc_task_die("Each dmac must be 'HH:HH:HH:HH:HH:HH'.");
      dmacs[num_links].octets[k] = strtol(&arg[k*3], NULL, 16);
    }

    num_links++;
    i += 2;
  }

  if (num_links == 0)
    tmc_task_die("FIXME: Usage!");

  // Simplify code below.
  link_names[num_links + 0] = "loop0";
  link_names[num_links + 1] = "loop1";


  // Check the links.
  for (i = 0; i < num_links + 2; i++)
  {
    // Get the instance.
    int old_instance = instance;
    instance = gxio_mpipe_link_instance(link_names[i]);
    if (instance < 0)
      tmc_task_die("Link '%s' does not exist.", link_names[i]);
    if (old_instance != instance && num_links > 0)
      tmc_task_die("Link '%s' uses different mPIPE instance.", link_names[i]);

    if (i < num_links)
    {
      uint8_t* b = &dmacs[i].octets[0];
      printf("Link %d (%s) [%d] dmac = '%02x:%02x:%02x:%02x:%02x:%02x'.\n",
             i, link_names[i], instance, b[0], b[1], b[2], b[3], b[4], b[5]);
    }
  }
  

  // Determine the available cpus.
  result = tmc_cpus_get_my_affinity(&cpus);
  VERIFY(result, "tmc_cpus_get_my_affinity()");

  if (tmc_cpus_count(&cpus) < num_links + 1)
    tmc_task_die("Insufficient cpus.");


  // Start the driver.
  result = gxio_mpipe_init(context, instance);
  VERIFY(result, "gxio_mpipe_init()");

  // Open the links.
  for (i = 0; i < num_links + 2; i++)
  {
    gxio_mpipe_link_t link;
    result = gxio_mpipe_link_open(&link, context, link_names[i], 0);
    VERIFY(result, "gxio_mpipe_link_open()");

    channels[i] = gxio_mpipe_link_channel(&link);
  }


  // Allocate some NotifRings.
  result = gxio_mpipe_alloc_notif_rings(context, num_links + 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_rings()");
  unsigned int ring = result;

  // Init the NotifRings.
  size_t notif_ring_entries = 512;
  size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);
  size_t needed = notif_ring_size + sizeof(gxio_mpipe_iqueue_t);
  for (int i = 0; i < num_links + 1; i++)
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


  // Allocate edma rings.
  result = gxio_mpipe_alloc_edma_rings(context, num_links + 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_edma_rings");
  uint edma = result;

  equeues = calloc(num_links + 1, sizeof(*equeues));
  if (equeues == NULL)
    tmc_task_die("Failure in 'calloc()'.");

  for (int i = 0; i < num_links + 1; i++)
  {
    // NOTE: The first egress queue uses "loop0", and the last egress
    // queue uses the "uplink" channel.
    int channel = channels[(i == 0) ? (num_links + 0) : (i % num_links)];
    size_t equeue_size = equeue_entries * sizeof(gxio_mpipe_edesc_t);
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_pagesize(&alloc, equeue_size);
    void* equeue_mem = tmc_alloc_map(&alloc, equeue_size);
    if (equeue_mem == NULL)
      tmc_task_die("Failure in 'tmc_alloc_map()'.");
    result = gxio_mpipe_equeue_init(&equeues[i], context, edma + i, channel,
                                    equeue_mem, equeue_size, 0);
    VERIFY(result, "gxio_mpipe_equeue_init()");
  }


  // Allocate one huge page to hold the buffer stack, and all the
  // packets.  This should be more than enough space.
  size_t page_size = tmc_alloc_get_huge_pagesize();
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* page = tmc_alloc_map(&alloc, page_size);
  assert(page);

  void* mem = page;


  // Allocate some NotifGroups.
  result = gxio_mpipe_alloc_notif_groups(context, num_links + 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_groups()");
  int group = result;

  // Allocate some buckets.
  int num_buckets = num_links + 1;
  result = gxio_mpipe_alloc_buckets(context, num_buckets, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  int bucket = result;


  for (int i = 0; i < num_links + 1; i++)
  {
    // Init groups and buckets, preserving packet order among flows.
    gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
    result = gxio_mpipe_init_notif_group_and_buckets(context, group + i,
                                                     ring + i, 1,
                                                     bucket + i, 1, mode);
    VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");
  }


  // ISSUE: How many buffers do we need?
  unsigned int num_buffers = 1000;


  // Allocate two buffer stacks.
  result = gxio_mpipe_alloc_buffer_stacks(context, 2, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buffer_stacks()");
  int stack_idx = result;

  // Initialize the buffer stacks.

  size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(num_buffers);
  gxio_mpipe_buffer_size_enum_t small_size = GXIO_MPIPE_BUFFER_SIZE_256;
  ALIGN(mem, 0x10000);
  result = gxio_mpipe_init_buffer_stack(context, stack_idx + 0, small_size,
                                        mem, stack_bytes, 0);
  mem += stack_bytes;

  gxio_mpipe_buffer_size_enum_t large_size = GXIO_MPIPE_BUFFER_SIZE_1664;
  ALIGN(mem, 0x10000);
  result = gxio_mpipe_init_buffer_stack(context, stack_idx + 1, large_size,
                                        mem, stack_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  mem += stack_bytes;

  // Register the entire huge page of memory which contains all the buffers.
  result = gxio_mpipe_register_page(context, stack_idx+0, page, page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");
  result = gxio_mpipe_register_page(context, stack_idx+1, page, page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");

  // Push some buffers onto the stacks.
  ALIGN(mem, 0x10000);
  for (int i = 0; i < num_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx + 0, mem);
    mem += 256;
  }
  ALIGN(mem, 0x10000);
  for (int i = 0; i < num_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx + 1, mem);
    mem += 1664;
  }

  // Paranoia.
  assert(mem <= page + page_size);


  // Register for packets.
  gxio_mpipe_rules_t rules;
  gxio_mpipe_rules_init(&rules, context);
  for (i = 0; i < num_links + 1; i++)
  {
    gxio_mpipe_rules_begin(&rules, bucket + i, 1, NULL);

    // Listen to the primary links.
    for (int k = 0; k < num_links; k++)
      gxio_mpipe_rules_add_channel(&rules, channels[k]);

    // Listen to "loop1" for packets from the local linux.
    gxio_mpipe_rules_add_channel(&rules, channels[num_links + 1]);

    // The non-catchall rules only handle a single dmac.
    if (i < num_links)
      gxio_mpipe_rules_add_dmac(&rules, dmacs[i]);
  }
  result = gxio_mpipe_rules_commit(&rules);
  VERIFY(result, "gxio_mpipe_rules_commit()");


  tmc_sync_barrier_init(&sync_barrier, num_links + 1);
  tmc_spin_barrier_init(&spin_barrier, num_links + 1);

  pthread_t threads[num_links];
  for (int i = 0; i < num_links; i++)
  {
    if (pthread_create(&threads[i], NULL, main_aux, (void*)(intptr_t)i) != 0)
      tmc_task_die("Failure in 'pthread_create()'.");
  }
  (void)main_aux((void*)(intptr_t)num_links);
  for (int i = 1; i < num_links; i++)
  {
    if (pthread_join(threads[i], NULL) != 0)
      tmc_task_die("Failure in 'pthread_join()'.");
  }

  // FIXME: Wait until pending egress is "done".
  for (int i = 0; i < 1000000; i++)
    __insn_mfspr(SPR_PASS);

  return 0;
}
