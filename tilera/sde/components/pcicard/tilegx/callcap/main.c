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

#include <asm/tilegxpci.h>
#include <tmc/task.h>
#include <tmc/sync.h>
#include <tmc/spin.h>

#include "local.h"

/******************************************************************************/
static tmc_sync_barrier_t syncbar;
static tmc_spin_barrier_t spinbar;

/******************************************************************************/
int main(int argc, char** argv)
/******************************************************************************/
{
   int i; /*Index*/
   pthread_t tid[2]; /*Processes ID*/

   tmc_sync_barrier_init(&syncbar, 2);
   tmc_spin_barrier_init(&spinbar, 2);

   if (pthread_create(&tid[0], NULL, h2t_thread, NULL) != 0) {
      tmc_task_die("Failed to create H2T thread\n");
   } 
   if (pthread_create(&tid[1], NULL, t2h_thread, NULL) != 0) {
      tmc_task_die("Failed to create T2H thread\n");
   } 
   for (i = 0; i < 2; i++) {
      pthread_join(tid[i], NULL);
   }
   printf("Done.\n");
   return 0;
}

