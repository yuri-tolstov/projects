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

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <asm/tilegxpci.h>

int main(int argc, char** argv)
{
  const char* ipath = "/dev/trio0-mac0/h2t/0";
  const char* opath = "/dev/trio0-mac0/t2h/0";
  int ifd, ofd;

  if ((ifd = open(ipath, O_RDWR)) < 0) {
    printf("Failed to open %s (errno=%d)\n", ipath, errno);
    abort();
  }
  printf("Opened %s\n", ipath);
  if ((ofd = open(opath, O_RDWR)) < 0) {
    printf("Failed to open %s (errno=%d)\n", opath, errno);
    abort();
  }
  printf("Opened %s\n", opath);
  close(ofd);
  close(ifd);

  return 0;
}

