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

/*----------------------------------------------------------------------------*/
#define MAXBATCH 16  /*Maximum number of packets in a batch*/

/*Global data.*/
extern gxio_mpipe_iqueue_t *iqueues[NUMLINKS]; /*mPIPE ingress queues*/
extern gxio_mpipe_equeue_t *equeues; /*mPIPE egress queues*/
extern cpu_set_t cpus; /*The initial affinity.*/

/******************************************************************************/
/* Thread:   net_thread                                                       */
/******************************************************************************/
void* net_thread(void* arg)
{
   int ifx = (uintptr_t)arg; /*Interface index*/
   int i, n; /*Index, Number*/
   gxio_mpipe_iqueue_t *iqueue = iqueues[ifx]; /*Ingress queue*/
   gxio_mpipe_equeue_t *equeue = &equeues[ifx]; /*Egress queue*/
   gxio_mpipe_idesc_t *idescs; /*Ingress packet descriptors*/
   gxio_mpipe_edesc_t edescs[MAXBATCH]; /*Egress descriptors.*/
   long slot;
  
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
   /*-------------------------------------------------------------------------*/
   /* Process(forward) packets.                                               */
   /*-------------------------------------------------------------------------*/
   while (1) {
      /*Receive packet(s).*/
      n = gxio_mpipe_iqueue_try_peek(iqueue, &idescs);
      if (n <= 0) continue;
      else if (n > 16) n = 16; //TODO: Experiment with this number.

      /*Prefetch packet descriptors from L3 to L1.*/
      tmc_mem_prefetch(idescs, n * sizeof(*idescs));

      /*Reserve slots.  NOTE: This might spin.*/
      slot = gxio_mpipe_equeue_reserve_fast(equeue, n);

      /*Prepair for output.*/
      for (i = 0; i < n; i++) {
         // idesc = &idescs[i];
         // edesc = &edescs[i];
         gxio_mpipe_edesc_copy_idesc(&edescs[i], &idescs[i]);
#if 0
         /*Drop "error" packets (but ignore "checksum" problems).*/
         if (idesc->be || idesc->me || idesc->tr || idesc->ce) {
            edesc->ns = 1;
         }
#endif
         /*Send the packets out*/
         gxio_mpipe_equeue_put_at(equeue, edescs[i], slot + i);
         gxio_mpipe_iqueue_consume(iqueue, &idescs[i]);
      }
   }
   /*Make compiler happy.*/
   return (void *)NULL;
}


