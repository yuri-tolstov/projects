/******************************************************************************/
/* File:   npu-ptempl/main.c                                                  */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <asm/tilegxpci.h>

/*****************************************************************************/
/* Thread:    H2T                                                            */
/*****************************************************************************/
static void* h2t_thread(void *arg)
{
   const char* path = "/dev/tilegxpci0/h2t/0";
   int fd; /*File descriptors*/
   void *mbuf; /*Output and Input Message buffers*/
   int msize = 4096; /*Message size*/
   int i; /*Index*/

   /*H2T channel.*/
   if ((fd = open(path, O_RDWR)) < 0) {
      printf("Failed to open '%s': %s\n", path, strerror(errno));
      return NULL;
   }
   printf("H2T: Opened.\n");
   mbuf = mmap(0, msize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   assert(mbuf != MAP_FAILED);

   /*Construct and initiate Send command*/
   tilepci_xfer_req_t sndcmd = {
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
   i = 1;
   while (1) {
      while (write(fd, &sndcmd, sizeof(sndcmd)) != sizeof(sndcmd));

      /*Read back the completion status.*/
      tilepci_xfer_comp_t comp;
      while (read(fd, &comp, sizeof(comp)) != sizeof(comp));

      printf("H2T: Written %d\n", i);
      sleep(10);
      i++;
   }
   /*Exit.*/
   printf("H2T: Done.\n");
   close(fd);
   return NULL;
}

/*****************************************************************************/
/* Thread:    T2H                                                            */
/*****************************************************************************/
static void* t2h_thread(void *arg)
{
   const char* path = "/dev/tilegxpci0/t2h/0";
   int fd; /*File descriptors*/
   void *mbuf; /*Output and Input Message buffers*/
   int msize = 4096; /*Message size*/
   int i; /*Index*/

   /*T2H channel.*/
   if ((fd = open(path, O_RDWR)) < 0) {
      printf("Failed to open '%s': %s\n", path, strerror(errno));
      return NULL;
   }
   printf("T2H: Opened.\n");
   mbuf = mmap(0, msize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
   assert(mbuf != MAP_FAILED);

   /*Construct and initiate Receive command*/
   tilepci_xfer_req_t rxcmd = {
      .addr = (uintptr_t)mbuf,
      .len = msize,
      .cookie = 0,
   };
   i = 1;
   while (1) {
      while (write(fd, &rxcmd, sizeof(rxcmd)) != sizeof(rxcmd));

      /*Read back the completion status.*/
      tilepci_xfer_comp_t comp;
      while (read(fd, &comp, sizeof(comp)) != sizeof(comp));

      printf("T2H: Received message %d\n", i);
      i++;
   }
   /*Exit.*/
   printf("T2H: Done.\n");
   close(fd);
   return NULL;
}

/*****************************************************************************/
/* Function:    main                                                         */
/*****************************************************************************/
int main(int argc, char *argv[])
{
   int i; /*Index*/
   pthread_t tid[2]; /*Processes ID*/

   if (pthread_create(&tid[0], NULL, h2t_thread, NULL) != 0) {
      printf("Failed to create H2T thread\n");
      return EIO;
   } 
   if (pthread_create(&tid[1], NULL, t2h_thread, NULL) != 0) {
      printf("Failed to create T2H thread\n");
      return EIO;
   } 
   for (i = 0; i < 2; i++) {
      pthread_join(tid[i], NULL);
   }
   printf("Done.\n");
   return 0;
}

