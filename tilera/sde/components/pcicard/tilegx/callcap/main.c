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

static tmc_sync_barrier_t syncbar;
static tmc_spin_barrier_t spinbar;
static char* lname[NUMLINKS]; /*Net links*/
static int chann[NUMLINKS]; /*Channels*/
static int mpipei; /*mPIPE instance*/
static gxio_mpipe_context_t mpipecd; /*mPIPE context (shared by all CPUs)*/
static gxio_mpipe_context_t* const mpipec = &mpipecd;

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
   /*Check links and get the mPIPE instance.*/
   for (i = 0; i < NUMLINKS; i++) {
      if ((c = gxio_mpipe_link_instance(lname[i])) < 0) {
         tmc_task_die("Link '%s' does not exist.", lname[i]);
      }
      if (mpipei == 0) mpipei = c;
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
      chann[i] = gxio_mpipe_link_channel(&link);
#if 0
      /*Allow to receive jumbo packets.*/
      gxio_mpipe_link_set_addr(&link, GXIO_MPIPE_LINK_RECEIVE_JUMBO, 1);
#endif
   }
   /*Allocate and initialize notification rings.*/
   unsigned int ring;
   size_t notif_ring_entries = 512;
   size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);
   size_t needed = notif_ring_size + sizeof(gxio_mpipe_iqueue_t);

   if ((ring = gxio_mpipe_alloc_notif_rings(mpipec, NUMNETTHRS, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. rings.");
   }
   for (int i = 0; i < NUMNETTHRS; i++) {
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
   /*Allocate NotifGroup.*/
   int group;
   if ((group = gxio_mpipe_alloc_notif_groups(mpipec, 1, 0, 0)) < 0) {
      tmc_task_die("Failed to alloc notif. groups.");
   }

  // Allocate some buckets. The default mPipe classifier requires
  // the number of buckets to be a power of two (maximum of 4096).
  int num_buckets = 1024;
  result = gxio_mpipe_alloc_buckets(mpipec, num_buckets, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  int bucket = result;

  // Init group and buckets, preserving packet order among flows.
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_DYNAMIC_FLOW_AFFINITY;
  result = gxio_mpipe_init_notif_group_and_buckets(mpipec, group,
                                                   ring, NUMNETTHRS,
                                                   bucket, num_buckets, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");






 









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

