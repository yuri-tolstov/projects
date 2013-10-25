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

/******************************************************************************/
#define WAITFOREVER (-1)

static tmc_sync_barrier_t syncbar;
static tmc_spin_barrier_t spinbar;

#if 0
/******************************************************************************/
static void* t2h_thread(void *arg)
/******************************************************************************/
{
   const char* path = "/dev/trio0-mac0/t2h/0";
   int fd; /*File Descriptor*/

   // tmc_sync_barrier_wait(&syncbar);

   if ((fd = open(path, O_RDWR)) < 0) {
      printf("Failed to open %s (errno=%d)\n", path, errno);
      abort();
   }
   printf("Opened %s\n", path);
#if 1
   while (1) {
      printf("T2H thread\n");
      sleep(5);
   }
#endif
   close(fd);
   return NULL;
}
#endif

/******************************************************************************/
static void* h2t_thread(void *arg)
/******************************************************************************/
{
   const char* path = "/dev/trio0-mac0/h2t/0";
   // int c, n; /*Return code, Number of bytes*/
   int c; /*Return code*/
   int fd; /*File descriptor*/
   char *mbuf; /*Message buffer*/
   int msize = 4096; /*Message size*/

   // tmc_sync_barrier_wait(&syncbar);
   /*Open the H2T channel.*/
   // if ((fd = open(path, O_RDWR)) < 0) {
   if ((fd = open(path, O_RDWR|O_NONBLOCK)) < 0) {
      printf("Failed to open %s (errno=%d)\n", path, errno);
      abort();
   }
   printf("Opened H2T\n");
   /*Allocate message buffer and register it to TRIO.*/
   mbuf = memalign(getpagesize(), msize);  assert(mbuf != NULL);
   tilegxpci_buf_info_t bufd = { /*Buffer descriptor*/
      .va = (uintptr_t)mbuf,
      .size = getpagesize(),
   };
   c = ioctl(fd, TILEPCI_IOC_REG_BUF, &bufd);  assert(c == 0);
#if 0
   /*Make file descriptor being non-blocking.*/
   int flags;
   flags = fcntl(fd, F_GETFL, 0);
   fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
   /*Construct receive command and post it to the PCI.*/
   tilepci_xfer_req_t rxcmd = { /*Rx Command descriptor*/
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
   while (write(fd, &rxcmd, sizeof(rxcmd)) != sizeof(rxcmd));
   printf("Written.\n");

   /*Read back the completion.*/
   tilepci_xfer_comp_t comp; /*Completion status*/
   while (read(fd, &comp, sizeof(comp)) != sizeof(comp));

   /*Processed the received data.*/
   printf("Got message\n");

   close(fd);
   return NULL;
}

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
#if 0
   if (pthread_create(&tid[1], NULL, t2h_thread, NULL) != 0) {
      tmc_task_die("Failed to create T2H thread\n");
   } 
#endif
   // for (i = 0; i < 2; i++) {
   for (i = 0; i < 1; i++) {
      pthread_join(tid[i], NULL);
   }
   printf("Done.\n");
   return 0;
}

