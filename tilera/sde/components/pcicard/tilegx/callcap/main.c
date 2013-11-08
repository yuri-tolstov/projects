/******************************************************************************/
/* File:   callcap/main.c                                                     */
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
#define NUMTHREADS (2/*Control*/ + NUMLINKS) /*Total number of threads*/

/*Align p mod align, assuming p is a void*/
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)

/*Globally visible data.*/
int mpipei; /*mPIPE instance*/
tmc_sync_barrier_t syncbar;
tmc_spin_barrier_t spinbar;
gxio_mpipe_iqueue_t *iqueues[NUMLINKS]; /*mPIPE ingress queues*/
gxio_mpipe_equeue_t *equeues; /*mPIPE egress queues*/

/*Locally visible data.*/
static char* lnames[NUMLINKS] = { /*Net links*/
   "gbe1", "gbe2", "gbe3", "gbe4"
};
static int channels[NUMLINKS]; /*Channels*/
static gxio_mpipe_context_t mpipecd; /*mPIPE context (shared by all CPUs)*/
static gxio_mpipe_context_t *mpipec = &mpipecd;
static unsigned int equeue_entries = 2048; /*The number of entries in the equeue ring.*/
static gxio_mpipe_rules_dmac_t dmacs[4]; /*The dmacs for each link.*/


/******************************************************************************/
int main(int argc, char** argv)
/******************************************************************************/
{
   int i, c, n; /*Index, Return code, Number*/
   cpu_set_t cpus; /*Initial CPU affinity*/
   pthread_t tid[NUMTHREADS]; /*Processes ID*/

   /*-------------------------------------------------------------------------*/
   /* Defaults and initialization.                                            */
   /*-------------------------------------------------------------------------*/
   mpipei = 0;
   tmc_sync_barrier_init(&syncbar, NUMLINKS);
   tmc_spin_barrier_init(&spinbar, NUMLINKS);

   /*Determine available CPUs.*/
   if (tmc_cpus_get_my_affinity(&cpus) < 0) {
      tmc_task_die("Failed to get affinity.");
   }
   if ((n = tmc_cpus_count(&cpus)) < NUMTHREADS) {
      tmc_task_die("Insufficient CPUs: %d.", n);
   }
   /*-------------------------------------------------------------------------*/
   /* mPIPE initialization.                                                   */
   /*-------------------------------------------------------------------------*/
   /*Check links and get the mPIPE instance.*/
   for (i = 0; i < NUMLINKS; i++) {
      if ((c = gxio_mpipe_link_instance(lnames[i])) < 0) {
         tmc_task_die("Link '%s' does not exist.", lnames[i]);
      }
      if (i == 0) mpipei = c;
      else if (c != mpipei) {
         tmc_task_die("Link '%s' uses diff. mPIPE instance.", lnames[i]);
      }
   }
   /*Start the driver.*/
   if (gxio_mpipe_init(mpipec, mpipei) < 0) {
      tmc_task_die("Failed to init mPIPE.");
   }
   /*Open the links.*/
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_link_t link;
      if (gxio_mpipe_link_open(&link, mpipec, lnames[i], 0) < 0) {
         tmc_task_die("Failed to open link %s.", lnames[i]);
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
   unsigned int iring;
   size_t iring_entries = 512;
   size_t iring_size = iring_entries * sizeof(gxio_mpipe_idesc_t);
   size_t needed = iring_size + sizeof(gxio_mpipe_iqueue_t);

   if ((iring = gxio_mpipe_alloc_notif_rings(mpipec, NUMLINKS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. rings.");
   }
   for (i = 0; i < NUMLINKS; i++) {
      void* iqueue_mem;
      tmc_alloc_t alloc = TMC_ALLOC_INIT;
      tmc_alloc_set_home(&alloc, tmc_cpus_find_nth_cpu(&cpus, i));
      /*The ring must use physically contiguous memory, but the iqueue
       *can span pages, so we use "iring_size", not "needed".*/
      tmc_alloc_set_pagesize(&alloc, iring_size);
      if ((iqueue_mem = tmc_alloc_map(&alloc, needed)) == NULL) {
         tmc_task_die("Failed tmc_alloc_map.");
      }
      gxio_mpipe_iqueue_t* iqueue = iqueue_mem + iring_size;
      if (gxio_mpipe_iqueue_init(iqueue, mpipec, iring + i,
                                 iqueue_mem, iring_size, 0) < 0) {
         tmc_task_die("Failed iqueue_init.");
      }
      iqueues[i] = iqueue;
   }
   /*-------------------------------------------------------------------------*/
   /* mPIPE egress path.                                                      */
   /*-------------------------------------------------------------------------*/
   uint ering;
   if ((ering = gxio_mpipe_alloc_edma_rings(mpipec, NUMLINKS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc eDMA rings.");
   }
   if ((equeues = calloc(NUMLINKS, sizeof(*equeues))) == NULL) {
      tmc_task_die("Failed in calloc.");
   }
   for (i = 0; i < NUMLINKS; i++) {
      size_t equeue_size = equeue_entries * sizeof(gxio_mpipe_edesc_t);
      tmc_alloc_t alloc = TMC_ALLOC_INIT;
      tmc_alloc_set_pagesize(&alloc, equeue_size);
      void* equeue_mem = tmc_alloc_map(&alloc, equeue_size);
      if (equeue_mem == NULL) {
         tmc_task_die("Failure in tmc_alloc_map.");
      }
      if (gxio_mpipe_equeue_init(&equeues[i], mpipec, ering + i, channels[i],
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

   /*-------------------------------------------------------------------------*/
   /* Notification groups.                                                    */
   /*-------------------------------------------------------------------------*/
   int group;
   if ((group = gxio_mpipe_alloc_notif_groups(mpipec, NUMLINKS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. groups.");
   }
   /*Allocate some buckets. The default mPIPE classifier requires
    *the number of buckets to be a power of two (maximum of 4096).*/
   int bucket;
   int nbuckets = NUMLINKS;
   if ((bucket = gxio_mpipe_alloc_buckets(mpipec, nbuckets, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc buckets.");
   }
   /*Map group and buckets, preserving packet order among flows.*/
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
      if (gxio_mpipe_init_notif_group_and_buckets(mpipec, group + i,
                                                  iring + i, NUMLINKS,
                                                  bucket + i, nbuckets, mode) < 0) {
         tmc_task_die("Failed to map groups and buckets.");
      }
   }
   /*-------------------------------------------------------------------------*/
   /* Buffers and Stacks.                                                     */
   /*-------------------------------------------------------------------------*/
   int stack_idx;
   gxio_mpipe_buffer_size_enum_t bufsize;
   unsigned int nbuffers = 1000;
   /*Allocate two buffer stacks.*/
   if ((stack_idx = gxio_mpipe_alloc_buffer_stacks(mpipec, 2, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc buffer stacks.");
   }
   /*Initialize the buffer stacks*/
   size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(nbuffers);
   bufsize = GXIO_MPIPE_BUFFER_SIZE_256;
   ALIGN(mem, 0x10000);
   if (gxio_mpipe_init_buffer_stack(mpipec, stack_idx + 0, bufsize,
                                    mem, stack_bytes, 0) < 0) {
      tmc_task_die("Failed to init buffer stack.");
   }
   mem += stack_bytes;
   bufsize = GXIO_MPIPE_BUFFER_SIZE_1664;
   ALIGN(mem, 0x10000);
   if (gxio_mpipe_init_buffer_stack(mpipec, stack_idx + 1, bufsize,
                                    mem, stack_bytes, 0) < 0) {
      tmc_task_die("Failed to init buffer stack.");
   }
   mem += stack_bytes;
   /*Register the entire huge page of memory which contains all the buffers.*/
   if (gxio_mpipe_register_page(mpipec, stack_idx + 0, page, page_size, 0) < 0) {
      tmc_task_die("Failed to register page.");
   }
   if (gxio_mpipe_register_page(mpipec, stack_idx + 1, page, page_size, 0) < 0) {
      tmc_task_die("Failed to register page.");
   }
   /*Push some buffers onto the stacks.*/
   ALIGN(mem, 0x10000);
   for (i = 0; i < nbuffers; i++) {
      gxio_mpipe_push_buffer(mpipec, stack_idx + 0, mem);
      mem += 256;
   }
   ALIGN(mem, 0x10000);
   for (i = 0; i < nbuffers; i++) {
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

   /*-------------------------------------------------------------------------*/
   /* Create and run working threads.                                         */
   /*-------------------------------------------------------------------------*/
printf("Creating threads...\n");
   /*Control threads.*/
   n = 0;
   if (pthread_create(&tid[n], NULL, h2t_thread, NULL) != 0) {
      tmc_task_die("Failed to create H2T thread\n");
   } 
   n++;
   if (pthread_create(&tid[n], NULL, t2h_thread, NULL) != 0) {
      tmc_task_die("Failed to create T2H thread\n");
   } 
   /*Net threads.*/
   for (i = 0, n++; i < NUMLINKS; i++, n++) {
      if (pthread_create(&tid[n], NULL, net_thread, (void *)(intptr_t)i) != 0) {
         tmc_task_die("Failed to create NET thread %d\n", i);
      } 
   }
printf("Done.\n");
   /*Wait for thread completion.*/
   for (i = 0; i < NUMTHREADS; i++) {
      pthread_join(tid[i], NULL);
   }
   printf("Done.\n");
   return 0;
}

