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

#include "app.h"

// Align "p" mod "align", assuming "p" is a "void*".
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while (0)

// macro to wrap a boolean with __builtin_expect
#define likely(cond) __builtin_expect((cond), 1)
#define unlikely(cond) __builtin_expect((cond), 0)


// Help check for errors.
#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)



//
// This set of defines determines the CompleteQ configuration.  The CompleteQ
// is used to re-establish packet order when workers are servicing NotifRings
// that are receiving their packets RoundRobin rather than affinitized.  The
// queue is written by the workers based on the sequence number of the packet.
// Dedicated "send" tiles process the queue in-order.  
//

// QUEUES_PER_SEND defines how many queues each sender manages.
#define QUEUES_PER_SEND 1

// This controls the size of the completion queue.  The queue holds 
// 2^^COMPQ_ORDER entries.  Each entry holds an edesc and a queue select.
#define COMPQ_ORDER 14
#define COMPQ_ENTRIES (1 << COMPQ_ORDER)
#define COMPQ_MASK (COMPQ_ENTRIES - 1)


// These defines control performance aspects of the application.
//

#define DEDICATED_MAINTANCENCE_TILE

#define NOTIF_RING_SIZE 512
#define WORKER_BATCH_SIZE (NOTIF_RING_SIZE >> 2)

// Mask defines errors for ME, TR, CE, BE in the idesc
#define IDESC_ERROR_MASK 0x008000004000C000

// In sticky mode, the fat-pipe idescs will burst to a single NotifRing until 
// the queue is full.  Thus the iqueue itself should be spread out in this case.
//#define USE_STICKY_MODE
#define HOME_IQUEUE_ON_WORKER
// Using multiple buckets requires that the classifier use the same sequence 
// number for all buckets.
#define NUM_BUCKETS 1

// Debug modes are helpful for finding app bugs.  Setting DEBUG to 1 will 
// display periodic status information if workers and/or senders are idle.
#define DEBUG 0

//#define STATS

#ifdef STATS
static volatile int start_cnt = 0;
#define START_STATS_PKT 120000000
#endif

// Help synchronize thread creation.
static tmc_sync_barrier_t all_sync_barrier;
static tmc_spin_barrier_t all_spin_barrier;
static tmc_spin_barrier_t* mcst_spin_barrier;
static tmc_spin_barrier_t worker_spin_barrier;

static volatile uint64_t last_cred_cons_ptr = 0;
static volatile uint64_t next_iqueue_cred_update = 0;

// The number of packets to process.
static int num_packets = -1;
static volatile int workers_complete = 0;

// The number of workers to use.
static unsigned int num_workers = 8;

// The number of senders to use.
#ifdef NUM_SENDERS
static unsigned int num_senders = NUM_SENDERS;
#else
static unsigned int num_senders = 1;
#endif

static unsigned int num_queues;

static int input_port_table[32];

static unsigned int initial_sqn;

static int bucket;

// The number of entries in the equeue ring.  
#define EQUEUE_ORDER 9
#define EQUEUE_ENTRIES (1 << EQUEUE_ORDER)

// This is the number of entries we reserve in the equeue whenever we need
// to send more packets.  
#define EQUEUE_RSV_CHUNK (EQUEUE_ENTRIES >> 1)

// The egress queues (one per sender).
static gxio_mpipe_equeue_t* equeues;

// Channel numbers for each link.
static int channels[16];

// Number of packets to inject into loopback channels if enabled.
static int startup_packets = 0;
static int startup_packet_size = 60;
static int startup_macs = 1;
static int stack_idx;

// Number of thousands of cycles to delay before starting the app.
static int startup_kcycles = 0;

// The initial affinity.
static cpu_set_t cpus;

// The mPIPE instance.
static int instance;

// The mpipe context (shared by all workers).
static gxio_mpipe_context_t context_body;
static gxio_mpipe_context_t* const context = &context_body;

// The ingress queues (one per worker).
static gxio_mpipe_iqueue_t** iqueues;

// The total number of packets forwarded by all workers.
static volatile unsigned long worker_total = 0;
static volatile unsigned long sender_total = 0;



#if DEBUG > 0
volatile static int mnum = 100;
#endif



// Consumed Pointers
static volatile uint64_t* cons_ptrs;

// The egress descriptors for the CompleteQueue go into compq_descs.  The 
// compq_vld bytes contain the valid (generation) bit as well as the queue
// selection.  The "send" tile sweeps teh compq_vld entries to find packets
// that can be egressed.
typedef uint8_t compq_vld_t;
static volatile gxio_mpipe_edesc_t* compq_descs;
static volatile compq_vld_t* compq_vld;

#define COMPQ_VLD_BITS_ORD 3
#define COMPQ_VLD_BITS (1 << COMPQ_VLD_BITS_ORD)
#define COMPQ_GEN_BIT (COMPQ_VLD_BITS - 1)
#define COMPQ_QSEL_MASK ((1 << COMPQ_GEN_BIT) - 1)

#define CV_VLD_PER_WD 8
#define CV_WD_PTR_MASK (CV_VLD_PER_WD - 1)
#define CV_COMP_MASK 0x8080808080808080
#define CV_COMP_MASK_EQ 0x0101010101010101
#define SEND_PREF_DEPTH 12

// Can do one link per QSEL bit
#define MAX_LINKS COMPQ_GEN_BIT
static const char* link_names[MAX_LINKS];

// Worker Synchronization
volatile uint64_t current_global_sqn;
volatile uint64_t* worker_pnd_gbl_sqn;
volatile uint64_t* worker_pnd_gp_sqn;
volatile int worker_wrap_sync_pending = 0;
volatile uint64_t base_gp_sqn;
  


#if DEBUG > 0
// This debug function is used to find a specific descriptor in the workers'
// NotifRings.
static int
debug_find_desc(uint64_t sqn)
{
  int found = 0;
  int nrs = num_workers;
  for (int i = 0; i < nrs; i++)
  {
    unsigned int head = iqueues[i]->head;
    printf("Ring %d: HEAD=0x%04x\n TAIL=0x%04x\n", 
           i, head, *((uint32_t *)(iqueues[i]->idescs)));
    for (int j = 1; j < iqueues[i]->num_entries; j++)
    {
      if (iqueues[i]->idescs[j].gp_sqn == (sqn & 0xffff))
      {
        found = 1;
        printf("  idesc[0x%04x] : GP=0x%04x PKT_SQN=0x%012lx\n",
               j, iqueues[i]->idescs[j].gp_sqn,
               (uint64_t)(iqueues[i]->idescs[j].packet_sqn));
      }
    }
  }
  return found;
}
#endif


// This function is used to send packets into a a loopback link when the test
// first starts up.
static int 
send_startup_packets(gxio_mpipe_equeue_t* equeue, int rank)
{
  int bstack = -1;

  gxio_mpipe_edesc_t edesc = {{ 0 }};
  edesc.xfer_size = startup_packet_size;
  edesc.hwb = 1;
  edesc.bound = 1;
    
  if (startup_packet_size <= 256)
  {
    bstack = stack_idx;
    edesc.size = GXIO_MPIPE_BUFFER_SIZE_256;
  }
  else if (startup_packet_size <= 1664)
  {
    bstack = stack_idx + 1;
    edesc.size = GXIO_MPIPE_BUFFER_SIZE_1664;
  }
  else
  {
    tmc_task_die("send_startup_packets size too large.");
  }

  edesc.stack_idx = bstack;

  int macs_per_sender = startup_macs;
  int total_macs =  num_senders * macs_per_sender;

  uint64_t my_base_mac = rank * macs_per_sender;
  uint64_t my_max_mac = my_base_mac + macs_per_sender - 1;

  char data_pattern = 0;
  uint64_t smac = my_base_mac;
  uint64_t dmac = 0;
  for (int i = 0; i < startup_packets; i++)
  {    
    void* buf = gxio_mpipe_pop_buffer(context, bstack);
    edesc.va = (uintptr_t)buf;
    if (dmac == my_base_mac)
    {
      dmac += macs_per_sender;
      if (dmac >= total_macs)
        dmac = 0;
    }
    // avoid bcst
    uint64_t x_dmac = dmac << 8;
    uint64_t x_smac = smac << 8;
    x_dmac |= (uint64_t)0xface << 32;
    x_smac |= (uint64_t)0xface << 32;
    ((uint64_t *)buf)[0] = x_dmac | (x_smac << 48);
    ((uint64_t *)buf)[1] = x_smac >> 16;

    for (int j = 12; j < (startup_packet_size - 12); j ++)
      ((char *)buf)[j] = data_pattern++;
    __insn_mf();
    gxio_mpipe_equeue_put(equeue, edesc);

    //    printf("%d sending S: %012lx  D: %012lx\n", rank, x_smac, x_dmac);

    if (++smac > my_max_mac)
      smac = my_base_mac;    
    if (++dmac >= total_macs)
      dmac = 0;
  }  
  return startup_packets & (2 * EQUEUE_ENTRIES - 1);
}

void sender_handle_mcst(gxio_mpipe_equeue_t* equeue,
                        int64_t slot,
                        gxio_mpipe_edesc_t edesc,
                        unsigned int qsel,
                        unsigned int qid)
{

#ifdef NO_MULTICAST
  assert(0);
#endif

  int64_t slot_mask = 1 << equeue->log2_num_entries;

  // I'm the LSB - I need to wait for other folks to sync
  int master = !(((1 << qid) - 1) & qsel);

  // barrier to use is the 1st unused port (assumes mcst/bcst always goes
  // to N-1 ports).
  int barrier_sel = __insn_ctz(~qsel);  
#if DEBUG > 0
  assert(barrier_sel < num_senders);
#endif

  if (master)
  {    
    tmc_spin_barrier_wait(&(mcst_spin_barrier[barrier_sel]));
    
    gxio_mpipe_equeue_put_at(equeue,
                             edesc, 
                             (slot ^ slot_mask));
    return;
  }

  void* buf = NULL;  

  if (edesc.va && edesc.hwb && edesc.xfer_size)
    buf = gxio_mpipe_pop_buffer(context, edesc.stack_idx);

  if (buf)
  {
    memcpy(buf, (unsigned char*)(intptr_t)edesc.va, edesc.xfer_size);
    edesc.va = (uintptr_t)buf;
    __insn_mf();
  }
  else
  {
    edesc.xfer_size = 0;
    edesc.ns = 1;
    edesc.hwb = 0;
#if DEBUG > 1
    printf("WARNING: NO MORE BUFFERS on %d.  %016lx_%016lx\n", edesc.stack_idx, 
           (uint64_t)(edesc.words[1]), (uint64_t)(edesc.words[0]));
#endif
  }

  gxio_mpipe_equeue_put_at(equeue,
			   edesc, 
			   (slot ^ slot_mask));


  tmc_spin_barrier_wait(&(mcst_spin_barrier[barrier_sel]));
}

// This is the main function for each send thread.
//
// ISSUE: A send tile could, in theory, be responsible for more than one queue. 
// GbE ports would be a good candidate since a dedicated sender per GbE
// would be excessive.  Some code fixes would be needed in this routine to
// handle more than one queue, but the basic support is there.  Would need to
// decode the qsel and select the proper equeue.  This should not hurt 
// performance since the SIMD operations for sweeping the COMPQ are designed
// for multiple queues.
static void*
main_aux_sender(void* arg)
{
  int result;

  int rank = (long)arg;
  // Pointer to the consumed-pointers that this sender is responsible for.
  // it is volatile to indicate that the workers will be sampleing these 
  // so they need to be kept up to date in shared memory.
  volatile uint64_t* my_cons_ptrs = &cons_ptrs[rank * QUEUES_PER_SEND];

  // The min/max_qid's indicate the range of quues this sender will handle.
  // If we wanted more than one equeue per sender, would need to add some code
  // below to select a specific equeue when sending.
  unsigned int min_qid = rank * QUEUES_PER_SEND;
  unsigned int max_qid = (rank * QUEUES_PER_SEND) + QUEUES_PER_SEND - 1;

  gxio_mpipe_equeue_t* equeue;
  int64_t slot = 0;
  int credits = 0;
  int handle_single_pkt = 0;
#if DEBUG > 0
  int p = 0;
#endif
#ifdef STATS
  unsigned long total_sent_count = 0;
  unsigned long loop_cnt = 0;
  unsigned long idle_cnt = 0;
  unsigned long no_cred_cnt = 0;
  unsigned long avg_batch_size = 0;
  uint64_t start_cyc = 0;
  uint64_t total_cycles = 0;
  unsigned long inner_loop_cnt = 0;
#endif

#if DEBUG > 0
  if (QUEUES_PER_SEND > 1)
    printf("Starting up sender %d with queues [%d..%d].\n", 
           rank, min_qid, max_qid);
  else
    printf("Starting up sender %d with queue %d.\n", 
           rank, min_qid);
#endif

  // Make sure generation bit fits in comp_vld structure
  assert(COMPQ_GEN_BIT < (sizeof(compq_vld_t) * 8));

  // Compiler doesn't do as good a job if we calculate this constant.  So make
  // sure it's right.
  assert(CV_WD_PTR_MASK == ((8 / sizeof(compq_vld_t)) - 1));
  
  // Bind to a dedicated cpu.
  result = tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, 
                               rank + num_workers));
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  mlockall(MCL_CURRENT);
  tmc_sync_barrier_wait(&all_sync_barrier);

  result = set_dataplane(0);
  VERIFY(result, "set_dataplane()");

  tmc_spin_barrier_wait(&all_spin_barrier);
  
  // This is my dedicated equeue.
  equeue = &equeues[rank];

  // If my channel is in loopback mode, inject some packets into the stream.
  // FIXME - MMIO read from MPIPE_LOOPBACK_MAP to get this.  Unfortunately,
  // that register is protected at level-2 so need an HV API.
  if (startup_packets)
  {
    printf("Sending %d startup packets (%dB) to channel[%d]\n", 
           startup_packets, startup_packet_size, channels[rank]);
    slot = send_startup_packets(equeue, rank);
  }

  // Setup dstream prefetcher
  __insn_mtspr(SPR_DSTREAM_PF, SEND_PREF_DEPTH << 6);

  // Initialize generation bit.
  slot += EQUEUE_ENTRIES; 
  slot &= (2 * EQUEUE_ENTRIES) - 1;

  int64_t slot_mask = 1 << equeue->log2_num_entries;

  uint64_t* edesc_p = (uint64_t *)equeue->edescs;

  // For the fast path code below, we use SIMD instructions to skip over 
  // multiple valid entries at the same time if they aren't for this "send"
  // tile.  Set a bit for each queue we pay attention to, then make a copy
  // in each simd chunk
  uint64_t comp_simd = 0;
  unsigned int min_qid_mask = (1 << min_qid);
  for (int i = min_qid; i <= max_qid; i++)
    comp_simd |= (uint64_t)1 << i;
  for (int i = 0; i < 64; i += COMPQ_VLD_BITS)
    comp_simd |= comp_simd << i;

#if DEBUG > 0
  printf("Sender %d will handle queues 0x%016lx\n", rank, comp_simd);
#endif

  while (workers_complete < num_workers)
  {
#ifdef STATS
    if (start_cnt)
      loop_cnt++;
#endif

#if DEBUG  > 0
    p++;
#endif
    if (credits < CV_VLD_PER_WD)
    {
#if DEBUG  > 0
      p = 1;
#endif
#ifdef STATS
      if (loop_cnt)
        no_cred_cnt++;
#endif
      do 
      {
        // We could just use gxio_mpipe_reserve_fast() here but for
        // debugging it's nice to see when the equeue is full.
        int64_t nslot =
          gxio_mpipe_equeue_try_reserve_fast(equeue, EQUEUE_RSV_CHUNK);
        credits += (nslot < 0) ? 0 : EQUEUE_RSV_CHUNK;

#if DEBUG  > 0
        if ((nslot < 0) && ((p & 0x3ffffff) == 0))
        {
          printf("Sender %d waiting for credit\n", rank);
          for (int i = 0; i < QUEUES_PER_SEND; i++)
          {
            printf("  Q:%d PTR:0x%08lx\n", i, my_cons_ptrs[i]);
          }
        }
        p++;
#endif
      } while (credits < CV_VLD_PER_WD);
    }
    
    uint64_t cons_ptr = my_cons_ptrs[0];
#if DEBUG > 0
    uint64_t old_cons_ptr = cons_ptr;
#endif
#ifdef STATS
    int old_credits = credits;
    int batch_cnt = 0;
    if (loop_cnt)
      start_cyc = __insn_mfspr(SPR_CYCLE);
#endif
    
    // Deal with "unaligned" consumed pointer in slower path code here.
    while ((credits != 0) && (handle_single_pkt || 
                              ((cons_ptr & CV_WD_PTR_MASK) != 0)))
    {
      compq_vld_t cv = compq_vld[cons_ptr & COMPQ_MASK];
      if (((cv >> COMPQ_GEN_BIT) ^
           (cons_ptr >> COMPQ_ORDER)) & 1)
      {
        unsigned int qsel = cv & COMPQ_QSEL_MASK;
        if (qsel & comp_simd)
        {
          
          // Detect mcst
          if ((qsel & min_qid_mask) != qsel)
            sender_handle_mcst(equeue,
                               slot,
                               compq_descs[cons_ptr & COMPQ_MASK],
                               qsel,
                               min_qid);
          else 
	    // Copy the descriptor from the CompQ to the equeue.
	    gxio_mpipe_equeue_put_at(equeue,
                                     compq_descs[cons_ptr & COMPQ_MASK], 
                                     (slot ^ slot_mask));
	  
	  slot++;
	  credits--;        
        }
        
        cons_ptr++;        
#if DEBUG > 0
        p = 1;
#endif
      }
      else
      {
#if DEBUG > 0
        if ((p++ & 0xfffffff) == 0)
        {
          printf("Sender %d waiting for work at 0x%08lx\n", 
                 rank, cons_ptr);
          if (rank == 0)
            debug_find_desc(cons_ptr);
        }
#endif
      }

      handle_single_pkt = 0;
    }

    assert(!credits || ((cons_ptr & CV_WD_PTR_MASK) == 0));

    for (int i = 4; i < SEND_PREF_DEPTH; i+=4)
      __insn_prefetch((void *)(&compq_descs[(cons_ptr + i) & COMPQ_MASK]));


    uint64_t av = 0;
    while (credits >= CV_VLD_PER_WD)
    {
#if SEND_PREF_DEPTH
      for (int i = 0; i < CV_VLD_PER_WD; i+=4)
        __insn_prefetch(
          (void *)(&compq_descs[(cons_ptr + SEND_PREF_DEPTH + i) & 
                                COMPQ_MASK]));

#endif
      // CompleteQueue entry is valid if the GEN bit is different
      // than the current pointer's gen bit.
      //
      // Check valid on multiple entries at a time to amortize cost.
      // The "while" above handles the case where we're not aligned to a
      // word boundary. 
      av = *((uint64_t *)&(compq_vld[cons_ptr & COMPQ_MASK]));
      uint64_t gmask = ((cons_ptr >> COMPQ_ORDER) & 1) ? 0 : CV_COMP_MASK;
      if ((av & CV_COMP_MASK) != gmask)
      {
        // Indicate that there is at least one valid descriptor we can
        // deal with.  This prevents us from stalling when there are 
        // fewer than 4 descriptors thus reducing the unloaded latency.
        handle_single_pkt = (((av ^ gmask) >> COMPQ_GEN_BIT) & 1) == 0;
        break;        
      }
      
      uint64_t av_sel = av & ~CV_COMP_MASK & comp_simd;


#if COMPQ_VLD_BITS == 8
      uint64_t compv = __insn_v1cmpeqi(av_sel, 0) ^ CV_COMP_MASK_EQ;
#elif COMPQ_VLD_BITS == 16
      uint64_t compv = __insn_v2cmpeqi(av_sel, 0) ^ CV_COMP_MASK_EQ;
#elif COMPQ_VLD_BITS == 32
      uint64_t compv = __insn_v4cmpeqi(av_sel, 0) ^ CV_COMP_MASK_EQ;
#else
      FAIL;
#endif


      uint64_t st_cons_ptr = cons_ptr;

      // Skip over any descriptors that aren't for me.
      int fv = __insn_ctz(compv);        
      while (fv < 64)
      {
        compv >>= fv;  
        av >>= fv;
        cons_ptr += (fv >> COMPQ_VLD_BITS_ORD);

        // Handle packets for me.  Slower path (per queue) code goes here.
        //
        // The fast-path code has ~4 spare cycles per iteration at 40Gbps 
        // min-packet on a 1.0GHz core.
        //
        // If the the code below handles a 1Gbps link, it would only be
        // called on average once every 40 iterations.  So it would have 
        // 40 * spare-cycles (~160 cycles) to perform its operations.
        //
        // A 10G link in a 40G system has 4 * spare-cycles = 16 cycles to 
        // perform the equeue_put_at function.
        //
        // A more typical system would be 2*20G full-duplex forwarding where 
        // this code would thus have ~24 spare cycles per cons_ptr.  Such
        // a system would typically contain two CompleteQ's, one for each 
        // direction.
        //
        // This code is typically where packets would be queued for TM/QoS
        // software or other application processing to be done on other Tiles.
        //
        volatile uint64_t* ew = 
          (uint64_t *)&compq_descs[cons_ptr & COMPQ_MASK];
        
        int wd_idx = (slot & (EQUEUE_ENTRIES - 1)) << 1;

        unsigned int qsel = av & COMPQ_QSEL_MASK;

          // Detect mcst
        if ((qsel & min_qid_mask) != qsel)
          sender_handle_mcst(equeue,
                             slot,
                             compq_descs[cons_ptr & COMPQ_MASK],
                             qsel, 
                             min_qid);
        else
	{
	  // These writes gaurantee order between word-1 and word-0.  It's
	  // not actually an MMIO write, but rather just a normal store.
	  __gxio_mmio_write64(&edesc_p[wd_idx+1], ew[1]);
	  __gxio_mmio_write64(&edesc_p[wd_idx+0], 
			      ew[0] | 
			      ((slot >> EQUEUE_ORDER) & 1));
	}
        
        slot++;
        cons_ptr++;        
        credits--;        
        compv >>= COMPQ_VLD_BITS;
        av >>= COMPQ_VLD_BITS;

        fv = __insn_ctz(compv);        

      }
      cons_ptr = st_cons_ptr + CV_VLD_PER_WD;
    }
     
#ifdef STATS
    batch_cnt = cons_ptr - old_cons_ptr;
    if (loop_cnt)
    {
      total_cycles += (__insn_mfspr(SPR_CYCLE) - start_cyc);
      inner_loop_cnt += batch_cnt;
      idle_cnt += (batch_cnt == 0) ? 1 : 0;
    }
    int sent_count = old_credits - credits;

    arch_atomic_add(&sender_total, sent_count);
    total_sent_count += sent_count;
#endif
  
    // Advance the consumed-pointers.
    for (int i = 0; i < QUEUES_PER_SEND; i++)
      my_cons_ptrs[i] = cons_ptr;      

    // Fence makes sure that our writes to the cons_ptrs and edescs are 
    // globally visible.
    // This should improve the unloaded latency.  A flush_wb would accomplish 
    // the same goal, but the mf actually provides slightly better 
    // performance.  This may be due to some interaction between loop 
    // iterations that benefits from the last iteration having completed all
    // of its memory ops.
    __insn_mf();
    

#if DEBUG > 0
    if (cons_ptr == old_cons_ptr)
    {
      if ((p & 0x3ffffff) == 0)
      {
        printf("Sender %d waiting for work at 0x%08lx\n", 
               rank, cons_ptr);
        if (rank == 0)
          debug_find_desc(cons_ptr);
      }
    }
    else 
    {
      p = 1;
    }
#endif            

#ifdef STATS
    if (loop_cnt)
      avg_batch_size += batch_cnt;
    
    start_cnt |= (total_sent_count > START_STATS_PKT);
#endif
  }

#if DEBUG > 0
  printf("Sender %d completed", rank);
#ifdef STATS
  printf(" %ld packets.\n", total_sent_count);
  printf(" SENDER IDLE_PCT:%0.4f  NO_CRED_PCT:%0.4f  AVG_BATCH:%0.4f\n",
         (float)idle_cnt/(float)loop_cnt,
         (float)no_cred_cnt/(float)loop_cnt,
         (float)avg_batch_size/(float)loop_cnt);
  printf("  VLD_PER_LOOP:%0.2f LOOP_CYCLES:%0.2f\n",
         (float)(total_sent_count - START_STATS_PKT)/(float)inner_loop_cnt,
         (float)total_cycles/(float)inner_loop_cnt);
         
#else
  printf(".\n");
#endif
#endif

  return (void*)NULL;
}


// Handle sequence number wrap by synchronizing the workers here.
// After synchronization, we can be sure that all workers are within
// 64K packets of each other.  This function returns the oldest 64-bit 
// sequence number in the system so workers can update their upper_sqn
// and last_sqn.  If a worker's SQN is (uint64_t)-1, it will appear
// "newer" than all other SQNs so will never be marked as oldest.
uint64_t
worker_sqn_wrap(int rank)
{
  // Let other workers know a sync is pending.  Multiple workers may
  // set this.
  worker_wrap_sync_pending = 1;

  // Make sure all older updates visible and force faster visibility of 
  // sync_pending.
  __insn_mf();

  // Wait here until all workers have processed their older packets.
  // FIXME - would be nice to have a periodic printf here if we're
  //         stuck waiting.
  tmc_spin_barrier_wait(&worker_spin_barrier);

  if (rank == 0)
  {
    // Update the global sequence number to be the oldest value in the system.
    current_global_sqn = worker_pnd_gbl_sqn[0]; 
    base_gp_sqn = worker_pnd_gp_sqn[0];
    for (int i = 1; i < num_workers; i++)
    {
      if (worker_pnd_gbl_sqn[i] < current_global_sqn)
      {
        current_global_sqn = worker_pnd_gbl_sqn[i];
        base_gp_sqn = worker_pnd_gp_sqn[i];
      }
    }
    worker_wrap_sync_pending = 0;
  }


  // Make sure all updates are coherent and the pending bit clear.  
  __insn_mf();

  // Sync here to make sure updates to current_global_sqn are done.
  tmc_spin_barrier_wait(&worker_spin_barrier);

  // Return full SQN of oldest pkt in system so worker can tell what
  // his upper_sqn is and when to wrap.
  return base_gp_sqn;
}

static void*
main_aux_maint(void* arg)
{
#ifdef DEDICATED_MAINTANCENCE_TILE
  unsigned long int maint_cycles = 2000000000;
  uint64_t next_maint_update = __insn_mfspr(SPR_CYCLE) + maint_cycles;

  int rank = (long)arg;
  int result = tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, 
                           rank + num_workers + num_senders));
  VERIFY(result, "tmc_cpus_set_my_cpu()");
  mlockall(MCL_CURRENT);
  tmc_sync_barrier_wait(&all_sync_barrier);

  result = set_dataplane(0);
  VERIFY(result, "set_dataplane()");

  tmc_spin_barrier_wait(&all_spin_barrier);
#else
  // FIXME - What is the right way to put this task onto the 
  // non-dataplane set?
  int result = tmc_cpus_set_my_cpu(0); 
  VERIFY(result, "tmc_cpus_set_my_cpu()");
  tmc_sync_barrier_wait(&all_sync_barrier);
  tmc_spin_barrier_wait(&all_spin_barrier);
#endif

  while(workers_complete < num_workers)
  {
    


#ifdef DEDICATED_MAINTANCENCE_TILE
    uint64_t curr_cycle = __insn_mfspr(SPR_CYCLE);
    if (curr_cycle < next_maint_update)
      continue;    
    next_maint_update = __insn_mfspr(SPR_CYCLE) + maint_cycles;
#else
    sleep(2);
#endif

    app_maintanance_operations();
  }

  return (void*)NULL;
}
 
// This is the main function for each worker thread.  The worker's job is to
// determine which send queue the packet goes in.  This would typically be
// based on a table lookup (L2/3 forwarding) and could involve policy lookups
// for traffic management and QoS.  
static void*
main_aux_worker(void* arg)
{
  int result;

  int rank = (long)arg;

  uint64_t compq_oldest_ptr = (initial_sqn & COMPQ_MASK) >> (COMPQ_ORDER - 1);
  unsigned int last_sqn = initial_sqn;
  uint64_t upper_sqn = 0;
  uint64_t current_global_sqn_msbs = 0;
  uint64_t pkt_gbl_sqn = 0;
  uint64_t sqn = 0;
  int sync_required = 0;
  uint64_t batch_sqns[WORKER_BATCH_SIZE];
  int batch_output_queues[WORKER_BATCH_SIZE];
#ifdef STATS
  unsigned int loop_cnt = 0;
  unsigned int no_cred_cnt = 0;
  unsigned int idle_cnt = 0;
  unsigned int avg_num_copied = 0;
  unsigned int drops = 0;
  unsigned int pkts = 0;
#endif
#if DEBUG > 0
  uint64_t tries = 0;
#endif

#if DEBUG > 0
  printf("Starting up worker %d.\n", rank);
#endif

  uint32_t out_queue_mask = (1 << num_queues) - 1;

  // Bind to a dedicated cpu.
  result = tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, 
                               rank));
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  tmc_sync_barrier_wait(&all_sync_barrier);

  result = set_dataplane(0/*DP_DEBUG | DP_POPULATE*/);
  VERIFY(result, "set_dataplane()");

  tmc_spin_barrier_wait(&all_spin_barrier);

  if (rank == 0)
  {
    // HACK: Pause briefly, to let everyone finish passing the barrier.
    for (int i = 0; i < 10000; i++)
      __insn_mfspr(SPR_PASS);

    // Allow packets to flow.
    sim_enable_mpipe_links(instance, -1);
  }


  // Forward packets.
  gxio_mpipe_iqueue_t* iqueue = iqueues[rank];

  while (1)
  {
#ifdef STATS
    if (start_cnt)
    {
      loop_cnt++;
    }
#endif
#if DEBUG > 0
    tries++;
    int stries = 0;
#endif

    // Handle periodic synchronization if required.
    while (unlikely(sync_required))
    {
#if DEBUG > 0
      if ((++stries & 0xffffff) == 0)
        printf("Worker %d waiting to sync to GBL=0x%08lx GP=0x%08lx\n", 
               rank, pkt_gbl_sqn, upper_sqn | (sqn & 0xffff));
#endif
      worker_pnd_gbl_sqn[rank] = pkt_gbl_sqn;
      worker_pnd_gp_sqn[rank] = upper_sqn | (sqn & 0xffff);
      // The worker_sqn_wrap function will wait for all the workers
      // to synchronize.
      uint64_t n_sqn = worker_sqn_wrap(rank);        
      upper_sqn = n_sqn & ~(uint64_t)0xffff;
      last_sqn  = n_sqn & 0xffff;

      // Make a local (nonvolatile) copy of the upper bits of the shared
      // global sequence number.
      current_global_sqn_msbs = current_global_sqn >> 16;

      // If the gbl_sqn is -1, we no longer need to sync because we are
      // empty.
      sync_required =
        (pkt_gbl_sqn != (uint64_t)-1 &&
         (pkt_gbl_sqn >> 16) != current_global_sqn_msbs);

      // The synchronization point is a good time to check if we're done
      // running the application.
      if (num_packets >= 0 && worker_total >= num_packets)
        goto done;
    }

    // Wait for packets to arrive.
    gxio_mpipe_idesc_t* idesc;
    int n = gxio_mpipe_iqueue_try_peek(iqueue, &idesc);
    if (likely(n > WORKER_BATCH_SIZE))
    {
      n = WORKER_BATCH_SIZE;
    }
    else if (unlikely(n <= 0))
    {
      // If my iqueue is empty and someone is waiting for a sync, then sync
      // up on next loop to tell them we don't have any older packets to work 
      // on.  This will update our upper_sqn if the other workers have wrapped
      // the sequence number space one or more times while we were idle.
      if (worker_wrap_sync_pending)
      {
        // If the quantization count is zero for our ring, then we're truly 
        // empty since tail==head and the load balancer also sees that.
        if (gxio_mpipe_iqueue_get_fullness(iqueue) == 0)
        {
          sync_required = 1;
          // Minus-1 tells sync routine we're idle.  We'll re-initialize our
          // sequence number when we get the next packet.
          pkt_gbl_sqn = (uint64_t)-1; 
          sqn = 0;
        }        
      }
      else if (!sync_required)
      {
        // ISSUE: Can this cause other workers to wedge?
        if (num_packets >= 0 && worker_total >= num_packets)
          goto done;
      }

#ifdef STATS
      if (loop_cnt)
        idle_cnt++;
#endif
#if DEBUG > 0
      if ((tries & 0xfffffff) == 0)
      {
        printf("Worker %d waiting for a packet at head=0x%04x.\n",
               rank, iqueue->head);
      }
#endif

      continue;
    }


#if WORKER_BATCH_SIZE > 1
    __insn_prefetch(&(idesc[1]));
    __insn_prefetch(&(idesc[2]));
    __insn_prefetch(&(idesc[3]));
    __insn_prefetch(&(idesc[4]));
    __insn_prefetch(&(idesc[5]));
#endif

    // Walk through the batch of descriptors and copy an edesc for
    // each one into the CompleteQueue.  If the CompleteQueue is out of credit,
    // we monitor the CompleteQueue-consumed pointers until credit is 
    // available.
    int num_copied;
    for (num_copied = 0; num_copied < n; num_copied++)
    {
#if WORKER_BATCH_SIZE > 1
      __insn_prefetch(&(idesc[num_copied + 6]));
#endif

      pkt_gbl_sqn = idesc[num_copied].packet_sqn;
      sqn = idesc[num_copied].gp_sqn;

      // Detect wrap locally and increment upper 48 bits of the sequence 
      // number.
      if (unlikely(sqn < last_sqn))
        upper_sqn += 0x10000;
      last_sqn = sqn;

      // Synchronize if global sequence number has incremented.
      if (unlikely((pkt_gbl_sqn >> 16) != current_global_sqn_msbs))
      {
        // A sync is required - we'll do it after marking our processed
        // descriptors as being completed (otherwise we could deadlock).
        sync_required = 1;
#if DEBUG > 1
        // Verify that the global sequence number does not go backwards.
        if ((pkt_gbl_sqn >> 16) < current_global_sqn_msbs)
        {
          tmc_task_die("Worker %d, GBL:0x%08lx < GBL_MSBs:0x%08lx"
                       " CURR_GBL:0x%08lx.",
                       rank, pkt_gbl_sqn, current_global_sqn_msbs << 16,
                       current_global_sqn);
        }
#endif
        break;
      }
      sqn |= upper_sqn;


#if DEBUG > 1
      // Note that this assert is only good for 2^48 packets (2 months 
      // @40Gbps).  So should not be used in production code.
      // Just checking to make sure that we don't mess up the upper_sqn.
      // This checker would catch the case where an idle worker synced just
      // before getting an older packet.  
      //
      // Note - this assertion only useful if we can get the initial gbl_sqn.
      // But there's no API for that.  Otherwise, it's only useful on the
      // first run after reset.
      if (sqn > pkt_gbl_sqn)
      {
        printf("ERROR: Worker %d, SQN:0x%08lx > GBL:0x%08lx\n",
               rank, sqn, pkt_gbl_sqn);
        assert (sqn <= pkt_gbl_sqn);
      }
#endif

      uint64_t compq_half_sel = (sqn >> (COMPQ_ORDER - 1));

      batch_sqns[num_copied] = sqn;

      int cant_go = compq_half_sel > (compq_oldest_ptr + 1);

#ifdef STATS
      if (loop_cnt)
      {
        if (cant_go)
          no_cred_cnt++;
      }
#endif
      // Need to update flow control and/or wait.
      if (unlikely(cant_go))
      {
#if DEBUG > 0
        if ((tries & 0x1ffffff) == 0)
        {
          printf("Worker %d waiting for credit to insert  0x%08lx.\n", 
                 rank, sqn);
          printf(" Oldest:0x%08lx  Me:0x%08lx\n", 
                 compq_oldest_ptr << (COMPQ_ORDER - 1), 
                 compq_half_sel << (COMPQ_ORDER - 1));
          printf(" SYNCP:%d GLBL:0x%08lx CMSBS:0x%08lx CGBL:0x%08lx\n",
                 worker_wrap_sync_pending, pkt_gbl_sqn, 
                 current_global_sqn_msbs, current_global_sqn);
          for (int j = 0; j < num_queues; j++)
          {
            printf("  Q:%d  PTR:0x%08lx\n", j, cons_ptrs[j]);
          }
        }
#endif
        compq_oldest_ptr = (cons_ptrs[0] >> (COMPQ_ORDER - 1));
        for (int j = 1; j < num_queues; j++)
        {
          uint64_t q_half_ptr = (cons_ptrs[j] >> (COMPQ_ORDER - 1));
          if (q_half_ptr < compq_oldest_ptr)
            compq_oldest_ptr = q_half_ptr;
        }

        // If there's still no credit for this, update.
        // the CompQ for any we HAVE written so we don't deadlock
        cant_go = compq_half_sel > (compq_oldest_ptr + 1);

      }
#if DEBUG > 0
      tries = 1;
#endif
      // If after updating credit we still can't make progress, set the valid
      // bits for the descriptors we were able to enqueue and try again.
      if (unlikely(cant_go))
      {
        break;
      }
      else 
      {
#ifdef STATS
        if (loop_cnt)
          pkts++;
#endif
        // Create an edesc and put it into the CompQ.
        gxio_mpipe_edesc_t edesc;
        gxio_mpipe_edesc_copy_idesc(&edesc, &idesc[num_copied]);

        // Make sure "be" packets just get dropped.
        // ISSUE: why is this ~20 cycles faster to do ourselves than to
        // call idesc_has_error??  Maybe unrelated code motion.
        if (unlikely(idesc[num_copied].words[0] & IDESC_ERROR_MASK))
        {
#ifdef STATS          
          if (loop_cnt)
          {
            if (!drops)
            {
              printf("Worker %d: first drop.  SQN:%ld\n",
                     rank, sqn);
            }
            drops++;
          }
#endif
          edesc.ns = 1;
          edesc.hwb = 0;
          edesc.xfer_size = 0;
          edesc.va = 0;
        }
#if DEBUG > 1
        // For debug, catch any cases where we're generating a bad edesc.
        // ISSUE: What about "va == headroom"?
        else if (unlikely(edesc.va == 0))
        {
          printf("Error in Worker %d: Bad edesc VA.\n", rank);
          for (int i = 0; i < 8; i++)
          {
            printf("  idesc[%d] : %016lx\n", i, 
                   *((uint64_t *)(&idesc[num_copied]) + i));
            
          }
          tmc_task_die("Worker %d got bad edesc VA.", rank);
        }
#endif
        compq_descs[sqn & COMPQ_MASK].words[0] = edesc.words[0];
        compq_descs[sqn & COMPQ_MASK].words[1] = edesc.words[1];
      }
    }

    for (int i = 0; i < num_copied; i++)
    {

      int input_port = input_port_table[idesc[i].channel];  
      
      int output_queue_select = app_handle_packet(&(idesc[i]), input_port);

      output_queue_select &= out_queue_mask;

      if (unlikely(output_queue_select == DROP_QUEUE))
      {
        // If packet needs to be dropped return the buffer manually.
        uint64_t sqn = batch_sqns[i];
        gxio_mpipe_edesc_t edesc = compq_descs[sqn & COMPQ_MASK];
        if (edesc.xfer_size && edesc.hwb)
        {
	  gxio_mpipe_push_buffer(context, edesc.stack_idx, 
				 (void *)((uintptr_t)(edesc.va)));	  
        }
      }
#if DEBUG > 1
      else 
      {
        assert((output_queue_select < (1 << num_queues)) &&
               (output_queue_select > 0));
      }
#endif

      batch_output_queues[i] = output_queue_select;

    }

    // Make sure the stores to the CompQ are globally visible before
    // setting the valid bits below.  Also make sure that any packet
    // updates performed by the application are visible.
    __insn_mf();

#ifdef STATS
    if (loop_cnt)
      avg_num_copied += num_copied;
#endif

#if DEBUG > 1
    // Use cons_ptrs[0] to do sqn range checks below.  This is a potentially
    // expensive operation since it will inval senders' cons_ptrs on each 
    // update.
    uint64_t c0 = cons_ptrs[0];
#endif

    for (int i = 0; i < num_copied; i++)
    {
      int output_queue_select = batch_output_queues[i]; 
      uint64_t sqn = batch_sqns[i];

#if DEBUG > 1
      // Check for sequence number within the queue window, at least based
      // on cons_ptrs[0].
      if (sqn < c0) 
      {
        tmc_task_die("Underrun on worker %d: SQN=0x%08lx CONS0=0x%08lx.",
                     rank, sqn, c0);
      }
      else if ((sqn - c0) >= COMPQ_ENTRIES)
      {
        tmc_task_die("Overrun on worker %d: SQN=0x%08lx CONS0=0x%08lx.",
                     rank, sqn, c0);
      }
#endif


      // Set the gen bit using the inverted size+1 bit of sqn.
      compq_vld_t nv;
      int gen_bit = (~(sqn >> COMPQ_ORDER)) & 1;

#if DEBUG > 1
      // Check that each time we write a CompqVld entry, it is inverting the
      // gen bit.
      int curr_gen_bit = (compq_vld[sqn & COMPQ_MASK] >> COMPQ_GEN_BIT) & 1;
      if (curr_gen_bit == gen_bit)
      {
        tmc_task_die("GEN-match on worker %d: SQN=0x%08lx.",
                     rank, sqn);
      }
#endif

      nv = (gen_bit << COMPQ_GEN_BIT) | output_queue_select;
      compq_vld[sqn & COMPQ_MASK] = nv;


#if NUM_BUCKETS > 1
      gxio_mpipe_iqueue_consume(iqueue, &(idesc[i]));
#endif
    }    

    // There's a code dependency from the idesc accesses above so
    // we don't need an MF here to gaurantee that the descriptors are
    // not overwritten.  The memory order model insures that we won't
    // perform the MMIO store in mpipe_credit prior to doing all the 
    // above loads to the idesc's.

    // Advance the idesc pointer and return credit for this batch of 
    // descriptors.
#if NUM_BUCKETS == 1
    gxio_mpipe_iqueue_advance(iqueue, num_copied);
    gxio_mpipe_credit(iqueue->context, iqueue->ring, bucket, num_copied);
#endif

    // Count packets (atomically).
    if (unlikely(num_packets >= 0))
      arch_atomic_add(&worker_total, num_copied);

    // Back off if we weren't able to make progress.  This should reduce the
    // communication between worker and sender thus reducing the coherence
    // traffic in the system.  
#if WORKER_BATCH_SIZE > 2
    if (unlikely(num_copied < (WORKER_BATCH_SIZE / 2)))
    {
      uint64_t start_cyc = __insn_mfspr(SPR_CYCLE);
      while (__insn_mfspr(SPR_CYCLE) < (start_cyc + 100));
    }
#endif
  }
 done:
  arch_atomic_add(&workers_complete, 1);

#if DEBUG > 0
  printf("Worker %d completed.  %ld (total) packets.\n", 
         rank, worker_total);
#ifdef APP_STATS
  if (rank == 0)
    app_stats();
#endif
#endif
#ifdef STATS
  printf("  WORKER PKTS:%d BE: %d IDLE:%02f  NO_CRED:%0.2f AVG_CPY:%0.2f\n",
         pkts,
         drops,
         (float)idle_cnt/(float)loop_cnt,
         (float)no_cred_cnt/(float)loop_cnt,
         (float)avg_num_copied/(float)loop_cnt);
         
#endif

  return (void*)NULL;
}


int
main(int argc, char** argv)
{
  int result;

  int num_links = 0;

  // Parse args.
  for (int i = 1; i < argc; i++)
  {
    char* arg = argv[i];

    if (!strcmp(arg, "--link") && i + 1 < argc)
    {
      if (num_links >= MAX_LINKS)
        tmc_task_die("Too many links!");
      link_names[num_links++] = argv[++i];
    }
    else if (!strcmp(arg, "-n") && i + 1 < argc)
    {
      num_packets = atoi(argv[++i]);
    }
    else if (!strcmp(arg, "-w") && i + 1 < argc)
    {
      num_workers = atoi(argv[++i]);
    }
    else if (!strcmp(arg, "-s") && i + 1 < argc)
    {
      startup_packets = atoi(argv[++i]);
    }
    else if (!strcmp(arg, "-l") && i + 1 < argc)
    {
      startup_packet_size = atoi(argv[++i]);
    }
    else if (!strcmp(arg, "-m") && i + 1 < argc)
    {
      startup_macs = atoi(argv[++i]);
    }
    else if (!strcmp(arg, "-c") && i + 1 < argc)
    {
      startup_kcycles = atoi(argv[++i]);
    }
    else
    {
      tmc_task_die("Unknown option '%s'.", arg);
    }
  }

  // Number of senders can be more than number of links if the application
  // wants to provide more queues per link.
  num_senders = num_links;

  int num_maint = 1;

  for (int i = 0; i < 32; i++)
    input_port_table[i] = -1;

  // Determine the available cpus.
  result = tmc_cpus_get_my_affinity(&cpus);
  VERIFY(result, "tmc_cpus_get_my_affinity()");

  if (tmc_cpus_count(&cpus) < (num_workers + num_senders + num_maint))
    tmc_task_die("Insufficient cpus.");


  // Get the instance.
  for (int i = 0; i < num_links; i++)
  {
    int old_instance = instance;
    instance = gxio_mpipe_link_instance(link_names[i]);
    if (instance < 0)
      tmc_task_die("Link '%s' does not exist.", link_names[i]);
    if (old_instance != instance && i != 0)
      tmc_task_die("Link '%s' uses different mPIPE instance.", link_names[i]); 
  }

  // Start the driver.
  result = gxio_mpipe_init(context, instance);
  VERIFY(result, "gxio_mpipe_init()");

  // Open the links.
  for (int i = 0; i < num_links; i++)
  {
    gxio_mpipe_link_t link;
    int channel_num;
    result = gxio_mpipe_link_open(&link, context, link_names[i], 0);
    VERIFY(result, "gxio_mpipe_link_open()");

    //gxio_mpipe_link_wait(&link, -1);
    //printf("Link %s is up\n", link_names[i]);

    channel_num = gxio_mpipe_link_channel(&link);
    channels[i] = channel_num;
    // Build table of port (send queue) numbers
    input_port_table[channel_num] = i * QUEUES_PER_SEND;
  }


  // Allocate some iqueues.
  int num_nrings = num_workers;

  iqueues = calloc(num_nrings, sizeof(*iqueues));
  if (iqueues == NULL)
    tmc_task_die("Failure in 'calloc()'.");

  // Allocate some NotifRings.
  result = gxio_mpipe_alloc_notif_rings(context, num_nrings, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_rings()");
  unsigned int ring = result;

  // Init the NotifRings.  The apps that use reorder will likely use
  // round-robin or sticky modes for load balancing.  Thus the total NR size
  // available for mPIPE is effectively num_workers * NR-size before packets
  // will start getting dropped.  Using a smaller NR size here will reduce
  // the cache footprint of the app.  As a rule of thumb, if more than ~10
  // workers are being used on a 40G flow (doing RR or sticky) then the
  // NR size can be 128.  Otherwise 512 or even 2048 should be used to
  // absorb bursts of packets and latency hiccups on the workers.
  size_t notif_ring_entries = NOTIF_RING_SIZE;
  size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);
  // pad the end of the iqueue to allow application prefetches "off the end"
  size_t needed = notif_ring_size + sizeof(gxio_mpipe_iqueue_t) +    
    16 * sizeof(gxio_mpipe_idesc_t);
  for (int i = 0; i < num_nrings; i++)
  {
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
#ifdef HOME_IQUEUE_ON_WORKER
    tmc_alloc_set_home(&alloc, tmc_cpus_find_nth_cpu(&cpus, i + 1));
#endif
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


  // Allocate one huge page to hold the edma rings, the buffer stack,
  // and all the packets.  This should be more than enough space.
  size_t page_size = tmc_alloc_get_huge_pagesize();
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* page = tmc_alloc_map(&alloc, page_size);
  assert(page);

  void* mem = page;


  // Allocate a NotifGroup.
  result = gxio_mpipe_alloc_notif_groups(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_groups()");
  int group = result;

  // Allocate a single bucket so we get one sequence number space.
  int num_buckets = NUM_BUCKETS;
  result = gxio_mpipe_alloc_buckets(context, num_buckets, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  bucket = result;

  // Init group and bucket.  RoundRobin or Sticky are fine here since
  // the example app is going to put the packets into the original 
  // receive order.  STICKY will likely perform better than RR since it
  // will tend to provide bursts of orderred packets to each worker.
#ifdef USE_STICKY_MODE
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_STICKY_FLOW_LOCALITY;
#else
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_ROUND_ROBIN;
#endif
  result = gxio_mpipe_init_notif_group_and_buckets(context, group,
                                                   ring, num_nrings,
                                                   bucket, num_buckets, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");


  // Allocate an edma ring.
  result = gxio_mpipe_alloc_edma_rings(context, num_senders, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_edma_rings");
  uint edma = result;

  // Init edma rings.
  equeues = calloc(num_senders, sizeof(*equeues));
  if (equeues == NULL)
    tmc_task_die("Failure in 'calloc()'.");

  for (int i = 0; i < num_senders; i++)
  {
    // For this example, it's assumed that there is exactly one sender per 
    // link (e.g. per channel).
    size_t edma_ring_size = EQUEUE_ENTRIES * sizeof(gxio_mpipe_edesc_t);
    tmc_alloc_t alloc = TMC_ALLOC_INIT;
    tmc_alloc_set_home(&alloc, 
                       tmc_cpus_find_nth_cpu(&cpus, 
                       i + num_workers));
    tmc_alloc_set_pagesize(&alloc, edma_ring_size);
    void* equeue_mem = tmc_alloc_map(&alloc, edma_ring_size);
    if (equeue_mem == NULL)
      tmc_task_die("Failure in 'tmc_alloc_map()'.");
    result = gxio_mpipe_equeue_init(&equeues[i], context, edma + i, 
                                    channels[i],
                                    equeue_mem, edma_ring_size, 0);
           
    VERIFY(result, "gxio_gxio_equeue_init()");
  }

  // Allocate huge pages to hold buffers
  size_t buf_page_size = tmc_alloc_get_huge_pagesize();
  tmc_alloc_t allocp = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&allocp);

  void* small_buf_page = tmc_alloc_map(&allocp, buf_page_size);
  assert(small_buf_page);
  void* small_buf_mem = small_buf_page;

  void* large_buf_page = tmc_alloc_map(&allocp, buf_page_size);
  assert(large_buf_page);
  void* large_buf_mem = large_buf_page;

  // Use enough buffers to ensure we never run out.
  //  unsigned int num_buffers = num_senders * EQUEUE_ENTRIES + 
  //    num_workers * notif_ring_entries;
  unsigned int num_small_buffers = 62000;
  unsigned int num_large_buffers = 10000;

  // Allocate two buffer stacks.
  result = gxio_mpipe_alloc_buffer_stacks(context, 2, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buffer_stacks()");
  stack_idx = result;

  // Initialize the buffer stacks.
  ALIGN(small_buf_mem, 0x10000);
  size_t small_stack_bytes = 
    gxio_mpipe_calc_buffer_stack_bytes(num_small_buffers);
  gxio_mpipe_buffer_size_enum_t small_buf_size = GXIO_MPIPE_BUFFER_SIZE_256;
  result = gxio_mpipe_init_buffer_stack(context, stack_idx+0, small_buf_size,
                                        small_buf_mem, small_stack_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  small_buf_mem += small_stack_bytes;

  ALIGN(large_buf_mem, 0x10000);
  size_t large_stack_bytes = 
    gxio_mpipe_calc_buffer_stack_bytes(num_large_buffers);
  gxio_mpipe_buffer_size_enum_t large_buf_size = GXIO_MPIPE_BUFFER_SIZE_1664;
  result = gxio_mpipe_init_buffer_stack(context, stack_idx+1, large_buf_size,
                                        large_buf_mem, large_stack_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  large_buf_mem += large_stack_bytes;

  // Register the huge page which contains the buffers.
  result = gxio_mpipe_register_page(context, stack_idx+0, small_buf_page, 
                                    buf_page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");
  result = gxio_mpipe_register_page(context, stack_idx+1, large_buf_page, 
                                    buf_page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");


  ALIGN(small_buf_mem, 0x10000);

  // Push some buffers onto the stack.
  for (int i = 0; i < num_small_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx+0, small_buf_mem);
    small_buf_mem += 256;
  }

  ALIGN(large_buf_mem, 0x10000);
  for (int i = 0; i < num_large_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx+1, large_buf_mem);
    large_buf_mem += 1664;
  }

  // Initialize the CompletionQ structures.
  ALIGN(mem, 16);
  compq_descs = mem;
  mem += COMPQ_ENTRIES * sizeof(gxio_mpipe_edesc_t);
  compq_vld = mem;
  mem += COMPQ_ENTRIES * sizeof(compq_vld_t);

#if DEBUG > 0
  printf("Using %ld out of %ld bytes from mem\n", mem - page, page_size);
  printf("Allocated %d entry CompQ.  Descs at 0x%08lx, vld's at 0x%08lx\n",
         COMPQ_ENTRIES, (uint64_t)compq_descs, (uint64_t)compq_vld);
#endif

  // Paranoia.
  assert(mem <= page + page_size);
  assert(small_buf_mem <= small_buf_page + buf_page_size);
  assert(large_buf_mem <= large_buf_page + buf_page_size);

  // Find out what the current SQN is so we can initialize all of our pointers.
  // FIXME: The simulator does not yet support "gxio_mpipe_get_sqn()".
  if (sim_is_simulator())
    initial_sqn = 0;
  else
    initial_sqn = gxio_mpipe_get_sqn(context, bucket);

#if NUM_BUCKETS > 1
  initial_sqn = gxio_mpipe_get_sqn(context, 0); // requires custom classifier
#endif

  // Clear the completion-queue-valid indicators by setting the generation
  // numbers based on the initial sequence number.
  int initial_gen = ((initial_sqn >> COMPQ_ORDER) & 1);
  for (int i = initial_gen; i < COMPQ_ORDER; i++)
  {
    compq_vld[i] = initial_gen << COMPQ_GEN_BIT;
  }
  initial_gen ^= 1;
  for (int i = 0; i < initial_sqn; i++)
  {
    compq_vld[i] = initial_gen << COMPQ_GEN_BIT;
  }
  
  // Register for packets.
  gxio_mpipe_rules_t rules;
  gxio_mpipe_rules_init(&rules, context);
  gxio_mpipe_rules_begin(&rules, bucket, num_buckets, NULL);
  result = gxio_mpipe_rules_commit(&rules);
  VERIFY(result, "gxio_mpipe_rules_commit()");

  // Init the consumed-pointers.
  num_queues = num_senders * QUEUES_PER_SEND;
  cons_ptrs = calloc(num_queues, sizeof(uint64_t));
  if (cons_ptrs == NULL)
    tmc_task_die("Failure in 'calloc()'.");
  for (int i = 0; i < num_queues; i++)
    cons_ptrs[i] = initial_sqn & COMPQ_MASK;

  // Allocate the global sequence number trackers.
  worker_pnd_gbl_sqn = calloc(num_workers, sizeof(uint64_t));
  if (worker_pnd_gbl_sqn == NULL)
    tmc_task_die("Failure in 'calloc()'.");
  worker_pnd_gp_sqn = calloc(num_workers, sizeof(uint64_t));
  if (worker_pnd_gp_sqn == NULL)
    tmc_task_die("Failure in 'calloc()'.");

  app_init();
  mcst_spin_barrier = calloc(num_senders, sizeof(tmc_spin_barrier_t));
  if (mcst_spin_barrier == NULL)
    tmc_task_die("Failure in 'calloc()'.");
  for (int i = 0; i < num_senders; i++)
  {
    tmc_spin_barrier_init(&(mcst_spin_barrier[i]), num_senders - 1);
  }
  

  tmc_sync_barrier_init(&all_sync_barrier, 
                        num_workers + num_senders + num_maint);
  tmc_spin_barrier_init(&all_spin_barrier, 
                        num_workers + num_senders + num_maint);
  tmc_spin_barrier_init(&worker_spin_barrier, num_workers);

  

  if (startup_kcycles) {
    printf("App is about to start - waiting %0d Kcycles\n", (int)startup_kcycles);
    uint64_t start_cyc = __insn_mfspr(SPR_CYCLE);
    uint64_t startup_cycles = (uint64_t)startup_kcycles * (uint64_t)1000;
    while (__insn_mfspr(SPR_CYCLE) < (start_cyc + (uint64_t)startup_cycles));
  }

  pthread_t threads[num_workers + num_senders + num_maint];
  for (int i = 1; i < num_workers; i++)
  {
    if (pthread_create(&threads[i], NULL, main_aux_worker, 
                       (void*)(intptr_t)i) != 0)
      tmc_task_die("Failure in 'pthread_create()' for worker.");
  }
  for (int i = num_workers; i < (num_workers + num_senders); i++)
  {
    if (pthread_create(&threads[i], NULL, main_aux_sender, 
                       (void*)(intptr_t)(i - num_workers)) != 0)
      tmc_task_die("Failure in 'pthread_create()' for sender.");
  }
  for (int i = num_workers + num_senders; 
       i < (num_workers + num_senders + num_maint); i++)
  {
    if (pthread_create(&threads[num_workers + num_senders], 
                       NULL, main_aux_maint, 
        (void*)(intptr_t)(i - (num_workers + num_senders))) != 0)
      tmc_task_die("Failure in 'pthread_create()' formaint.");  
  }
  (void)main_aux_worker((void*)(intptr_t)0);
  for (int i = 1; i < num_workers; i++)
  {
    if (pthread_join(threads[i], NULL) != 0)
      tmc_task_die("Failure in 'pthread_join()' for worker.");
  }
  for (int i = num_workers; i < (num_workers + num_senders); i++)
  {
    if (pthread_join(threads[i], NULL) != 0)
      tmc_task_die("Failure in 'pthread_join()' for sender.");
  }
  for (int i = num_workers + num_senders; 
       i < (num_workers + num_senders + num_maint); i++)
  if (pthread_join(threads[i], NULL) != 0)
    tmc_task_die("Failure in 'pthread_join()' for maint.");


  // FIXME: Wait until pending egress is "done".
  for (int i = 0; i < 1000000; i++)
    __insn_mfspr(SPR_PASS);

  return 0;
}
