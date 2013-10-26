/******************************************************************************/
/* File:   ctrl.c                                                             */
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
void* t2h_thread(void *arg)
/******************************************************************************/
{
   const char* path = "/dev/trio0-mac0/t2h/0";
   // int c, n; /*Return code, Number of bytes*/
   int i; /*Index*/
   int c; /*Return code*/
   int fd; /*File descriptor*/
   char *mbuf; /*Message buffer*/
   int msize = 4096; /*Message size*/

   // tmc_sync_barrier_wait(&syncbar);
   /*Open the T2H channel.*/
   if ((fd = open(path, O_RDWR)) < 0) {
   // if ((fd = open(path, O_RDWR|O_NONBLOCK)) < 0) {
      printf("Failed to open %s (errno=%d)\n", path, errno);
      abort();
   }
   printf("T2H: Opened\n");
   /*Allocate message buffer and register it to TRIO.*/
   mbuf = memalign(getpagesize(), msize);  assert(mbuf != NULL);
   tilegxpci_buf_info_t bufd = { /*Buffer descriptor*/
      .va = (uintptr_t)mbuf,
      .size = getpagesize(),
   };
   c = ioctl(fd, TILEPCI_IOC_REG_BUF, &bufd);  assert(c == 0);

   /*Construct Send command and post it to the PCI.*/
   tilepci_xfer_req_t txcmd = { /*TX Command descriptor*/
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
   i = 1;
   while (1) {
      while (write(fd, &txcmd, sizeof(txcmd)) != sizeof(txcmd));

      /*Read back the completion.*/
      tilepci_xfer_comp_t comp; /*Completion status*/
      while (read(fd, &comp, sizeof(comp)) != sizeof(comp));

      printf("T2H: Sent message %d\n", i);
      sleep(30);
      i++;
   }
   close(fd);
   return NULL;
}

/******************************************************************************/
void* h2t_thread(void *arg)
/******************************************************************************/
{
   const char* path = "/dev/trio0-mac0/h2t/0";
   // int c, n; /*Return code, Number of bytes*/
   int i; /*Index*/
   int c; /*Return code*/
   int fd; /*File descriptor*/
   char *mbuf; /*Message buffer*/
   int msize = 4096; /*Message size*/

   // tmc_sync_barrier_wait(&syncbar);
   /*Open the H2T channel.*/
   if ((fd = open(path, O_RDWR)) < 0) {
   // if ((fd = open(path, O_RDWR|O_NONBLOCK)) < 0) {
      printf("Failed to open %s (errno=%d)\n", path, errno);
      abort();
   }
   printf("H2T: Opened\n");
   /*Allocate message buffer and register it to TRIO.*/
   mbuf = memalign(getpagesize(), msize);  assert(mbuf != NULL);
   tilegxpci_buf_info_t bufd = { /*Buffer descriptor*/
      .va = (uintptr_t)mbuf,
      .size = getpagesize(),
   };
   c = ioctl(fd, TILEPCI_IOC_REG_BUF, &bufd);  assert(c == 0);

   /*Construct receive command and post it to the PCI.*/
   tilepci_xfer_req_t rxcmd = { /*Rx Command descriptor*/
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
   i = 1;
   while (1) {
      while (write(fd, &rxcmd, sizeof(rxcmd)) != sizeof(rxcmd));

      /*Read back the completion.*/
      tilepci_xfer_comp_t comp; /*Completion status*/
      while (read(fd, &comp, sizeof(comp)) != sizeof(comp));

      printf("H2T: Received message %d\n", i);
      i++;
   }
   close(fd);
   return NULL;
}

