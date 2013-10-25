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

/******************************************************************************/
static void* t2h_thread(void *arg)
/******************************************************************************/
{
   const char* path = "/dev/trio0-mac0/t2h/0";
   int fd; /*File Descriptor*/

   tmc_sync_barrier_wait(&syncbar);

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

/******************************************************************************/
static void* h2t_thread(void *arg)
/******************************************************************************/
{
   const char* path = "/dev/trio0-mac0/h2t/0";
   int c, n; /*Return code, Number of bytes*/
   int fd; /*File descriptor*/
   char *mbuf; /*Message buffer*/
   int msize = 4096; /*Message size*/

   tmc_sync_barrier_wait(&syncbar);
   /*Open the channel.*/
   if ((fd = open(path, O_RDWR)) < 0) {
      printf("Failed to open %s (errno=%d)\n", path, errno);
      abort();
   }
   printf("Opened %s\n", path);
   /*Allocate message buffer and register it to TRIO.*/
   mbuf = memalign(getpagesize(), msize);
   tilegxpci_buf_info_t bufd = { /*Buffer descriptor*/
      .va = (uintptr_t)mbuf,
      .size = getpagesize(),
   };
   if (ioctl(fd, TILEPCI_IOC_REG_BUF, &bufd) != 0) {
      printf("Failed to ioctl on %s (errno=%d)\n", path, errno);
      abort();
   }
   /*Initialize the Message.*/
   tilepci_xfer_req_t msgd = { /*Message descriptor*/
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
#if 1
   struct pollfd fds; /*Poll descriptor*/
   memset(&fds, 0, sizeof(fds)); 
   fds.fd = fd;
   fds.events = POLLIN;
   while (1) {
      // c = poll(&fds, 1, WAITFOREVER); /*Wait until an event arrives*/
      c = poll(&fds, 1, 3000); /*Wait for event during 1 sec.*/
      if (c < 0) {
         printf("Poll failed (errno=%d)\n", errno);
         break;
      }
      else if (c == 0) {
         n = write(fd, &msgd, sizeof(msgd));
         if (n != sizeof(msgd)) {
            printf("Write failed (n=%d errno=%d)\n", n, errno);
         }
         else {
            printf("Write succeded. n=%d errno=%d\n", n, errno);
         }
      }
      else {
         if (fds.events == POLLIN) {
             printf("Poll: c=%d\n", c);
         }
      }
   }
#endif
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
  if (pthread_create(&tid[1], NULL, t2h_thread, NULL) != 0) {
     tmc_task_die("Failed to create T2H thread\n");
  } 
  for (i = 0; i < 2; i++) {
     pthread_join(tid[i], NULL);
  }
   return 0;
}

