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
#if 1
   const char* ipath = "/dev/tilegxpci0/t2h/0";
   const char* opath = "/dev/tilegxpci0/h2t/0";
#else
   const char* ipath = "/dev/tilegxpci0/0";
   const char* opath = "/dev/tilegxpci0/1";
#endif
   int ifd, ofd; /*File descriptors*/

   if ((ofd = open(opath, O_RDWR)) < 0) {
      printf("Failed to open '%s': %s\n", opath, strerror(errno));
      return 2;
   }
   printf("Opened h2t.\n");
   if ((ifd = open(ipath, O_RDWR)) < 0) {
      printf("Failed to open '%s': %s\n", ipath, strerror(errno));
      return 1;
   }
   printf("Opened t2h.\n");
   close(ifd);
   close(ofd);
   return 0;
}

