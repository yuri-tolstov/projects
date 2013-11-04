/******************************************************************************/
/* File:   main.c                                                             */
/******************************************************************************/
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/dataplane.h>

#include <asm/tilegxpci.h>
#include <arch/atomic.h>

#include <gxio/mpipe.h>
#include <tmc/mem.h>
#include <tmc/alloc.h>
#include <tmc/task.h>
#include <tmc/sync.h>
#include <tmc/spin.h>
#include <tmc/cpus.h>

#include "local.h"

/******************************************************************************/
#define NAMELEN  32  /*Name length*/
#define NUMLINKS  4  /*Number of network interfaces*/
#define NUMNETTHRS  4  /*Number of network threads*/
#define NUMTHREADS (2/*Control*/ + NUMNETTHRS) /*Total number of threads*/

/*Align p mod align, assuming p is a void*/
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)

static tmc_sync_barrier_t syncbar;
static tmc_spin_barrier_t spinbar;
static char* lname[NUMLINKS]; /*Net links*/
static int channels[NUMLINKS]; /*Channels*/
static int mpipei; /*mPIPE instance*/
static gxio_mpipe_context_t mpipecd; /*mPIPE context (shared by all CPUs)*/
static gxio_mpipe_context_t* const mpipec = &mpipecd;
static gxio_mpipe_iqueue_t* iqueues[NUMLINKS]; /*mPIPE ingress queues*/
static gxio_mpipe_equeue_t* equeues; /*mPIPE egress queues*/
static unsigned int equeue_entries = 2048; /*The number of entries in the equeue ring.*/
static gxio_mpipe_rules_dmac_t dmacs[4]; /*The dmacs for each link.*/



/******************************************************************************/
int main(int argc, char** argv)
/******************************************************************************/
{
   int i, c, tno; /*Index, Return code, Thread number*/
   cpu_set_t cpus; /*Initial CPU affinity*/
   pthread_t tid[NUMTHREADS]; /*Processes ID*/
   // char name[NAMELEN]; /*Link name*/

   /*-------------------------------------------------------------------------*/
   /* Defaults and initialization.                                            */
   /*-------------------------------------------------------------------------*/
   lname[0] = "gbe0"; lname[1] = "gbe1"; lname[2] = "gbe2"; lname[3] = "gbe3";
   mpipei = 0;
   tmc_sync_barrier_init(&syncbar, 2);
   tmc_spin_barrier_init(&spinbar, 2);

   /*Determine available CPUs.*/
   if (tmc_cpus_get_my_affinity(&cpus) < 0) {
      tmc_task_die("Failed to get affinity.");
   }
   if (tmc_cpus_count(&cpus) < NUMNETTHRS) {
      tmc_task_die("Insufficient cpus.");
   }
   /*-------------------------------------------------------------------------*/
   /* mPIPE initialization.                                                   */
   /*-------------------------------------------------------------------------*/
   /*Check links and get the mPIPE instance.*/
   for (i = 0; i < NUMLINKS; i++) {
      if ((c = gxio_mpipe_link_instance(lname[i])) < 0) {
         tmc_task_die("Link '%s' does not exist.", lname[i]);
      }
      if (i == 0) mpipei = c;
      else if (c != mpipei) {
         tmc_task_die("Link '%s' uses diff. mPIPE instance.", lname[i]);
      }
   }
   /*Start the driver.*/
   if (gxio_mpipe_init(mpipec, mpipei) < 0) {
      tmc_task_die("Failed to init mPIPE.");
   }
   /*Open the links.*/
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_link_t link;
      if (gxio_mpipe_link_open(&link, mpipec, lname[i], 0) < 0) {
         tmc_task_die("Failed to open link %s.", lname[i]);
      } 
      channels[i] = gxio_mpipe_link_channel(&link);
#if 0
      /*Allow to receive jumbo packets.*/
      gxio_mpipe_link_set_addr(&link, GXIO_MPIPE_LINK_RECEIVE_JUMBO, 1);
#endif
   }
   /*-------------------------------------------------------------------------*/
   /* mPIPE ingress path.                                                     */
   /*-------------------------------------------------------------------------*/
   /*Allocate and initialize notification rings.*/
   unsigned int ring;
   size_t notif_ring_entries = 512;
   size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);
   size_t needed = notif_ring_size + sizeof(gxio_mpipe_iqueue_t);

   if ((ring = gxio_mpipe_alloc_notif_rings(mpipec, NUMNETTHRS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. rings.");
   }
   for (i = 0; i < NUMNETTHRS; i++) {
      tmc_alloc_t alloc = TMC_ALLOC_INIT;
      tmc_alloc_set_home(&alloc, tmc_cpus_find_nth_cpu(&cpus, i));
      /*The ring must use physically contiguous memory, but the iqueue
       *can span pages, so we use "notif_ring_size", not "needed".*/
      tmc_alloc_set_pagesize(&alloc, notif_ring_size);
      void* iqueue_mem = tmc_alloc_map(&alloc, needed);
      if (iqueue_mem == NULL) {
         tmc_task_die("Failed tmc_alloc_map.");
      }
      gxio_mpipe_iqueue_t* iqueue = iqueue_mem + notif_ring_size;
      if ((c = gxio_mpipe_iqueue_init(iqueue, mpipec, ring + i,
                                      iqueue_mem, notif_ring_size, 0)) < 0) {
         tmc_task_die("Failed iqueue_init.");
      }
      iqueues[i] = iqueue;
   }
#if 0
   /*Allocate NotifGroup.*/
   int group;
   if ((group = gxio_mpipe_alloc_notif_groups(mpipec, 1, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. groups.");
   }
   /*Allocate some buckets. The default mPipe classifier requires
    *the number of buckets to be a power of two (maximum of 4096).*/
   int bucket;
   int num_buckets = 1024;
   if (( bucket = gxio_mpipe_alloc_buckets(mpipec, num_buckets, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc buckets.");
   }
   /*Map group and buckets, preserving packet order among flows.*/
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
      if (gxio_mpipe_init_notif_group_and_buckets(mpipec, group,
                                                  ring, NUMNETTHRS,
                                                  bucket, num_buckets, mode) < 0) {
         tmc_task_die("Failed to map groups and buckets.");
      }
   }
#endif
   /*-------------------------------------------------------------------------*/
   /* mPIPE egress path.                                                      */
   /*-------------------------------------------------------------------------*/
   uint edma;
   if ((edma = gxio_mpipe_alloc_edma_rings(mpipec, NUMLINKS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc eDMA rings.");
   }
   if ((equeues = calloc(NUMLINKS, sizeof(*equeues))) == NULL) {
      tmc_task_die("Failed in calloc.");
   }
   for (int i = 0; i < NUMLINKS; i++) {
      size_t equeue_size = equeue_entries * sizeof(gxio_mpipe_edesc_t);
      tmc_alloc_t alloc = TMC_ALLOC_INIT;
      tmc_alloc_set_pagesize(&alloc, equeue_size);
      void* equeue_mem = tmc_alloc_map(&alloc, equeue_size);
      if (equeue_mem == NULL) {
         tmc_task_die("Failure in tmc_alloc_map.");
      }
      if (gxio_mpipe_equeue_init(&equeues[i], mpipec, edma + i, channels[i],
                                 equeue_mem, equeue_size, 0) < 0) {
         tmc_task_die("Failed in mpipe_enqueue_init.");
      }
   }
   /*Allocate one huge page to hold the buffer stack, and all the
    *packets. This should be more than enough space.*/
   size_t page_size = tmc_alloc_get_huge_pagesize();
   tmc_alloc_t alloc = TMC_ALLOC_INIT;
   tmc_alloc_set_huge(&alloc);
   void* page = tmc_alloc_map(&alloc, page_size);  assert(page);
   void* mem = page;
#if 1
   /*-------------------------------------------------------------------------*/
   /* Notification groups.                                                    */
   /*-------------------------------------------------------------------------*/
   int group;
   if ((group = gxio_mpipe_alloc_notif_groups(mpipec, 1, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. groups.");
   }
   /*Allocate some buckets. The default mPipe classifier requires
    *the number of buckets to be a power of two (maximum of 4096).*/
   int bucket;
   int num_buckets = 1024; //NUMLINKS
   if ((bucket = gxio_mpipe_alloc_buckets(mpipec, num_buckets, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc buckets.");
   }
   /*Map group and buckets, preserving packet order among flows.*/
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
      if (gxio_mpipe_init_notif_group_and_buckets(mpipec, group,
                                                  ring, NUMNETTHRS,
                                                  bucket, num_buckets, mode) < 0) {
         tmc_task_die("Failed to map groups and buckets.");
      }
   }
#endif
   /*-------------------------------------------------------------------------*/
   /* Buffers and Stacks.                                                     */
   /*-------------------------------------------------------------------------*/
   int stack_idx;
   /*Allocate two buffer stacks.*/
   if ((stack_idx = gxio_mpipe_alloc_buffer_stacks(mpipec, 2, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc buffer stacks.");
   }
   /*Initialize the buffer stacks*/
   unsigned int num_buffers = 1000;
   size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(num_buffers);
   gxio_mpipe_buffer_size_enum_t small_size = GXIO_MPIPE_BUFFER_SIZE_256;
   ALIGN(mem, 0x10000);
   if (gxio_mpipe_init_buffer_stack(mpipec, stack_idx + 0, small_size,
                                    mem, stack_bytes, 0) < 0) {
      tmc_task_die("Failed to init buffer stack.");
   }
   mem += stack_bytes;
   gxio_mpipe_buffer_size_enum_t large_size = GXIO_MPIPE_BUFFER_SIZE_1664;
   ALIGN(mem, 0x10000);
   if (gxio_mpipe_init_buffer_stack(mpipec, stack_idx + 1, large_size,
                                    mem, stack_bytes, 0) < 0) {
      tmc_task_die("Failed to init buffer stack.");
   }
   mem += stack_bytes;

   /*Register the entire huge page of memory which contains all the buffers.*/
   if (gxio_mpipe_register_page(mpipec, stack_idx+0, page, page_size, 0) < 0) {
      tmc_task_die("Failed to register page.");
   }
   if (gxio_mpipe_register_page(mpipec, stack_idx+1, page, page_size, 0) < 0) {
      tmc_task_die("Failed to register page.");
   }
   /*Push some buffers onto the stacks.*/
   ALIGN(mem, 0x10000);
   for (i = 0; i < num_buffers; i++) {
      gxio_mpipe_push_buffer(mpipec, stack_idx + 0, mem);
      mem += 256;
   }
   ALIGN(mem, 0x10000);
   for (i = 0; i < num_buffers; i++) {
      gxio_mpipe_push_buffer(mpipec, stack_idx + 1, mem);
      mem += 1664;
   }
   /*-------------------------------------------------------------------------*/
   /* Register for packets.                                                   */
   /*-------------------------------------------------------------------------*/
   gxio_mpipe_rules_t rules;
   gxio_mpipe_rules_init(&rules, mpipec);
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_rules_begin(&rules, bucket + i, 1, NULL);
      /*Listen to the primary links.*/  //TODO: What it is for?
      for (int k = 0; k < NUMLINKS; k++) {
         gxio_mpipe_rules_add_channel(&rules, channels[k]);
      }
      /*The non-catchall rules only handle a single dmac.*/
      if (i < NUMLINKS) {
         gxio_mpipe_rules_add_dmac(&rules, dmacs[i]);
      }
   }
   if (gxio_mpipe_rules_commit(&rules) < 0) {
      tmc_task_die("Failed to commit rules.");
   }
   /*Line up.*/
   tmc_sync_barrier_init(&syncbar, NUMLINKS);
   tmc_spin_barrier_init(&spinbar, NUMLINKS);

   /*-------------------------------------------------------------------------*/
   /* Create and run working threads.                                         */
   /*-------------------------------------------------------------------------*/
   /*Control threads.*/
   tno = 0;
   if (pthread_create(&tid[tno], NULL, h2t_thread, NULL) != 0) {
      tmc_task_die("Failed to create H2T thread\n");
   } 
   tno++;
   if (pthread_create(&tid[tno], NULL, t2h_thread, NULL) != 0) {
      tmc_task_die("Failed to create T2H thread\n");
   } 
   /*Net threads.*/
   for (i = 0, tno++; i < NUMNETTHRS; i++, tno++) {
      if (pthread_create(&tid[tno], NULL, net_thread, (void *)(intptr_t)i) != 0) {
         tmc_task_die("Failed to create NET thread %d\n", i);
      } 
   }
   /*Wait for thread completion.*/
   for (i = 0; i < NUMTHREADS; i++) {
      pthread_join(tid[i], NULL);
   }
   printf("Done.\n");
   return 0;
}

