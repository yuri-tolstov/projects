/******************************************************************************/
/* File:   callcap/main.c                                                     */
/******************************************************************************/
#include "local.h"

/******************************************************************************/
#define NAMELEN  32  /*Name length*/
#define NUMTHREADS (2/*Control*/ + NUMLINKS) /*Total number of threads*/

/*Align p mod align, assuming p is a void*/
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)

/*Globally visible data.*/
cpu_set_t cpus; /*Initial CPU affinity*/
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
static gxio_mpipe_context_t mpipecontext; /*mPIPE context (shared by all CPUs)*/
static gxio_mpipe_context_t *mpipecon = &mpipecontext;
static unsigned int equeue_entries = 2048; /*The number of entries in the equeue ring.*/
// static gxio_mpipe_rules_dmac_t dmacs[4]; /*The dmacs for each link.*/

static gxio_trio_context_t triocontext; /*TRIO context*/
static gxio_trio_context_t* triocon = &triocontext;
static gxpci_context_t h2tcontext;
static gxpci_context_t t2hcontext;
static gxpci_context_t *h2tcon = &h2tcontext;
static gxpci_context_t *t2hcon = &t2hcontext;
static int triox = 0;
static int queuex = 0;
static gxpci_queue_type_t queuetype = GXPCI_PQ_DUPLEX;
static int vfunx; /*Virtual funciton number*/
static int locmac; /*Local MAC index*/
static int asid = GXIO_ASID_NULL;

/******************************************************************************/
int main(int argc, char** argv)
/******************************************************************************/
{
   int i, c, n; /*Index, Return code, Number*/
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
   if (gxio_mpipe_init(mpipecon, mpipei) < 0) {
      tmc_task_die("Failed to init mPIPE.");
   }
   /*Open ingress channels.*/
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_link_t link;
      if (gxio_mpipe_link_open(&link, mpipecon, lnames[i], 0) < 0) {
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

   if ((iring = gxio_mpipe_alloc_notif_rings(mpipecon, NUMLINKS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. rings.");
   }
   for (i = 0; i < NUMLINKS; i++) {
      void* iqueue_mem;
      tmc_alloc_t alloc = TMC_ALLOC_INIT;
      tmc_alloc_set_home(&alloc, tmc_cpus_find_nth_cpu(&cpus, DTILEBASE + i));
      /*The ring must use physically contiguous memory, but the iqueue
       *can span pages, so we use "iring_size", not "needed".*/
      tmc_alloc_set_pagesize(&alloc, iring_size);
      if ((iqueue_mem = tmc_alloc_map(&alloc, needed)) == NULL) {
         tmc_task_die("Failed tmc_alloc_map.");
      }
      gxio_mpipe_iqueue_t* iqueue = iqueue_mem + iring_size;
      if (gxio_mpipe_iqueue_init(iqueue, mpipecon, iring + i,
                                 iqueue_mem, iring_size, 0) < 0) {
         tmc_task_die("Failed iqueue_init.");
      }
      iqueues[i] = iqueue;
   }
   /*-------------------------------------------------------------------------*/
   /* mPIPE egress path.                                                      */
   /*-------------------------------------------------------------------------*/
   uint ering;
   if ((ering = gxio_mpipe_alloc_edma_rings(mpipecon, NUMLINKS, 0, 0)) < 0) {
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
      if (gxio_mpipe_equeue_init(&equeues[i], mpipecon, ering + i, channels[i],
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
   if ((group = gxio_mpipe_alloc_notif_groups(mpipecon, NUMLINKS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. groups.");
   }
   /*Allocate some buckets. The default mPIPE classifier requires
    *the number of buckets to be a power of two (maximum of 4096).*/
   int bucket;
   int nbuckets = NUMLINKS + 1;
   if ((bucket = gxio_mpipe_alloc_buckets(mpipecon, nbuckets, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc buckets.");
   }
   /*Map groups and buckets, preserving packet order among flows.*/
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
      if ((c = gxio_mpipe_init_notif_group_and_buckets(mpipecon, group + i, iring + i, 1,
                                                  bucket + i, 1, mode)) < 0) {
         tmc_task_die("Failed to map groups and buckets (%s)", gxio_strerror(c));
      }
   }
   /*-------------------------------------------------------------------------*/
   /* Buffers and Stacks.                                                     */
   /*-------------------------------------------------------------------------*/
   int stack;
   gxio_mpipe_buffer_size_enum_t bufsize;
   unsigned int nbuffers = 1000;
   /*Allocate two buffer stacks.*/
   if ((stack = gxio_mpipe_alloc_buffer_stacks(mpipecon, 2, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc buffer stacks.");
   }
   /*Initialize the buffer stacks*/
   size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(nbuffers);
   bufsize = GXIO_MPIPE_BUFFER_SIZE_256;
   ALIGN(mem, 0x10000);
   if (gxio_mpipe_init_buffer_stack(mpipecon, stack + 0, bufsize,
                                    mem, stack_bytes, 0) < 0) {
      tmc_task_die("Failed to init buffer stack.");
   }
   mem += stack_bytes;
   bufsize = GXIO_MPIPE_BUFFER_SIZE_1664;
   ALIGN(mem, 0x10000);
   if (gxio_mpipe_init_buffer_stack(mpipecon, stack + 1, bufsize,
                                    mem, stack_bytes, 0) < 0) {
      tmc_task_die("Failed to init buffer stack.");
   }
   mem += stack_bytes;
   /*Register the entire huge page of memory which contains all the buffers.*/
   if (gxio_mpipe_register_page(mpipecon, stack + 0, page, page_size, 0) < 0) {
      tmc_task_die("Failed to register page.");
   }
   if (gxio_mpipe_register_page(mpipecon, stack + 1, page, page_size, 0) < 0) {
      tmc_task_die("Failed to register page.");
   }
   /*Push some buffers onto the stacks.*/
   ALIGN(mem, 0x10000);
   for (i = 0; i < nbuffers; i++) {
      gxio_mpipe_push_buffer(mpipecon, stack + 0, mem);
      mem += 256;
   }
   ALIGN(mem, 0x10000);
   for (i = 0; i < nbuffers; i++) {
      gxio_mpipe_push_buffer(mpipecon, stack + 1, mem);
      mem += 1664;
   }
   /*-------------------------------------------------------------------------*/
   /* Register for packets.                                                   */
   /*-------------------------------------------------------------------------*/
   gxio_mpipe_rules_t rules;
   gxio_mpipe_rules_init(&rules, mpipecon);
   for (i = 0; i < NUMLINKS; i++) {
      gxio_mpipe_rules_begin(&rules, bucket + i, 1, NULL);
      gxio_mpipe_rules_add_channel(&rules, channels[i]);
#if 0
      /*The non-catchall rules only handle a single dmac.*/
      if (i < NUMLINKS) {
         gxio_mpipe_rules_add_dmac(&rules, dmacs[i]);
      }
#endif
   }
   if (gxio_mpipe_rules_commit(&rules) < 0) {
      tmc_task_die("Failed to commit rules.");
   }
   /*-------------------------------------------------------------------------*/
   /* Setup Tile-Host communication channel.                                  */
   /*-------------------------------------------------------------------------*/
   if (gxio_trio_init(triocon, triox) != 0) {
      tmc_task_die("gxio_trio_init failed.");
   }
   if (gxpci_init(triocon, h2tcon, triox, locmac) != 0) {
      tmc_task_die("gxpci_init(H2T) failed.");
   }
   if (gxpci_init(triocon, t2hcon, triox, locmac) != 0) {
      tmc_task_die("gxpci_init(T2H) failed.");
   }
   if (gxpci_open_duplex_queue(h2tcon, t2hcon, asid, queuetype, vfunx, queuex) != 0) {
      tmc_task_die("gxpci_open_duplex_queue failed.");
   }
   /*-------------------------------------------------------------------------*/
   /* Create and run control and network threads.                             */
   /*-------------------------------------------------------------------------*/
   /*Control threads/tasks.*/
   n = 0;
   if (pthread_create(&tid[n], NULL, h2t_thread, NULL) != 0) {
      tmc_task_die("Failed to create H2T thread\n");
   } 
   n++;
   if (pthread_create(&tid[n], NULL, t2h_thread, NULL) != 0) {
      tmc_task_die("Failed to create T2H thread\n");
   } 
   n++;
   /*Net threads/tasks.*/
   for (i = 0; i < NUMLINKS; i++, n++) {
      if (pthread_create(&tid[n], NULL, net_thread, (void *)((intptr_t)i)) != 0) {
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

