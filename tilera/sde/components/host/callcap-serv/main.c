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
/* Function:    main                                                         */
/*****************************************************************************/
int main(int argc, char *argv[])
{
   const char* h2tpath = "/dev/tilegxpci0/h2t/0";
   // const char* t2hpath = "/dev/tilegxpci0/t2h/0";
   // int h2tfd, t2hfd; /*File descriptors*/
   int h2tfd; /*File descriptors*/
   // void *h2tbuf, *t2hbuf; /*Output and Input Message buffers*/
   void *h2tbuf; /*Output and Input Message buffers*/
   int msize = 4096; /*Message size*/
   // int n; /*Number*/

   /*H2T channel.*/
   if ((h2tfd = open(h2tpath, O_RDWR)) < 0) {
      printf("Failed to open '%s': %s\n", h2tpath, strerror(errno));
      return 1;
   }
   printf("Opened T2H.\n");
   h2tbuf = mmap(0, msize, PROT_READ|PROT_WRITE, MAP_SHARED, h2tfd, 0);
   assert(h2tbuf != MAP_FAILED);
   printf("Mapped T2H.\n");

   /*Construct and initiate Send command*/
   tilepci_xfer_req_t sndcmd = {
      .addr = (uintptr_t)h2tbuf,
      .len = msize,
      .cookie = 0,
   };
   while (write(h2tfd, &sndcmd, sizeof(sndcmd)) != sizeof(sndcmd));
   printf("Written\n");

   /*Read back the completion status.*/
   tilepci_xfer_comp_t comp;
   while (read(h2tfd, &comp, sizeof(comp)) != sizeof(comp));
   
   /*Exit.*/
   printf("Done.\n");
   close(h2tfd);
   return 0;
}

