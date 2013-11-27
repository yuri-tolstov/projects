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
// Linux client side of the BME client-server example.  This program
// writes blocks of data into a shared memory page and then asks the
// server to process them by sending it messages over the UDN.
//

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asm/bme.h>

#include <linux/types.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <tmc/cpus.h>
#include <tmc/udn.h>
#include <tmc/alloc.h>

#include "msg.h"


// Bytes of data we'll process per request to the server.  Since this is an
// example, this is intentionally low; we want to see multiple processing
// messages flowing with a small input file.
#define PROCESSING_BUFSIZE 1024


int
main(int argc, char** argv)
{
  // Process arguments.

  int i = 1;

  while (i < argc)
  {
    // Allow "-i FILE" to override STDIN.
    if (i + 2 <= argc && !strcmp(argv[i], "-i"))
    {
      const char* file = argv[i+1];
      if (dup2(open(file, O_RDONLY), STDIN_FILENO) < 0)
      {
        fprintf(stderr, "Could not open '%s'.\n", file);
        exit(1);
      }
      i += 2;
    }

    // Allow "-o FILE" to override STDOUT.
    else if (i + 2 <= argc && !strcmp(argv[i], "-o"))
    {
      const char* file = argv[i+1];
      int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (dup2(fd, STDOUT_FILENO) < 0)
      {
        fprintf(stderr, "Could not open '%s'.\n", file);
        exit(1);
      }
      i += 2;
    }

    else
    {
      break;
    }
  }

  // Get the UDN coordinates of the BME server tile from our arguments.
  int server_x, server_y;
  if (i + 1 != argc || sscanf(argv[i], "%d,%d", &server_x, &server_y) != 2)
  {
    fprintf(stderr,
            "usage: linux_client [-i IN] [-o OUT] <server_x>,<server_y>\n");
    exit(1);
  }

  // Create a UDN header for the server.
  DynamicHeader bme_server =
    { .bits.dest_x = server_x, .bits.dest_y = server_y };


  // Bind ourselves to our current CPU, and set up a UDN hardwall
  // which encompasses the entire chip, so that we can communicate
  // with the BME server.

  cpu_set_t cpus;

  tmc_cpus_clear(&cpus);
  tmc_cpus_grid_add_all(&cpus);

  tmc_cpus_set_my_cpu(tmc_cpus_get_my_current_cpu());

  if (tmc_udn_init(&cpus) != 0)
  {
    perror("UDN hardwall create failed");
    exit(1);
  }

  if (tmc_udn_activate() != 0)
  {
    perror("UDN hardwall activate failed");
    exit(1);
  }


  // Get one huge page of memory.
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  tmc_alloc_set_home(&alloc, 0);
  tmc_alloc_set_shared(&alloc);
  int mlength = 1 << 24;
  void* maddr = tmc_alloc_map(&alloc, mlength);
  if (maddr == NULL)
  {
    perror("can't mmap");
    exit(1);
  }


  // Lock down that memory and get its physical address and caching
  // information, using the bme_mem device driver.

  struct bme_user_mem_desc_io user_mem_desc;
  struct bme_phys_mem_desc_io phys_mem_desc;
  int fd = open("/dev/bme/mem", O_RDWR);

  if (fd < 0)
  {
    perror("couldn't open /dev/bme/mem");
    exit(1);
  }


  // First we find out how many pages are in the region to be locked down.
  // (Given our allocation above, we know we must have exactly one large page,
  // but this is an example of what you would do for large regions.)

  //user_mem_desc.user.va = maddr;
  user_mem_desc.user.va = (uintptr_t)maddr;
  //  user_mem_desc.user.va = (__u64)maddr;
  user_mem_desc.user.len = mlength;

  if (ioctl(fd, BME_IOC_GET_NUM_PAGES, &user_mem_desc) != 0)
  {
    perror("BME_IOC_GET_NUM_PAGES ioctl failed");
    exit(1);
  }


  // Now that we know how many pages are there, we can request that they be
  // locked into physical memory, and retrieve their physical address and
  // cache mapping information.

  phys_mem_desc.user.va = (uintptr_t)maddr;
  phys_mem_desc.user.len = mlength;

  phys_mem_desc.phys =
    (uintptr_t)malloc(sizeof(struct bme_phys_mem_desc) *
                      user_mem_desc.num_pages);

  phys_mem_desc.num_pages = user_mem_desc.num_pages;

  if (ioctl(fd, BME_IOC_LOCK_MEMORY, &phys_mem_desc) != 0)
  {
    perror("BME_IOC_LOCK_MEMORY ioctl failed");
    exit(1);
  }


  // Send the BME application a message telling it about the memory we
  // just locked down.  Since this is an example, we're only sending one
  // message, for one page.

  DynamicHeader my_hdr = tmc_udn_header_from_cpu(tmc_cpus_get_my_cpu());

  struct bme_phys_mem_desc *phys =
    (struct bme_phys_mem_desc *)(uintptr_t)phys_mem_desc.phys;

  tmc_udn_send_6(bme_server, UDN0_DEMUX_TAG,
                 EX_MSG_MAPPING,
                 my_hdr.word,
                 phys->pa,
                 phys->pa >> 32,
                 phys->pte,
                 phys->pte >> 32);

  uint32_t reply = udn0_receive();
  if (reply)
  {
    fprintf(stderr, "client: got bad response %d to MAPPING message\n",
            reply);
    exit(1);
  }


  // Now read our standard input into a buffer in the shared page; send
  // a request to the BME tile to process that data, putting the output
  // elsewhere in the shared page; and then write it to standard output.

  char* inbuf = maddr;
  char* outbuf = inbuf + PROCESSING_BUFSIZE;
  
  int len;
  while ((len = read(STDIN_FILENO, inbuf, PROCESSING_BUFSIZE)) > 0)
  {
    // Note that our message gives the server the offsets of the input and
    // output buffers, rather than pointers to them.  This is because the
    // server has not mapped in the data at the same set of virtual addresses
    // we're using.  We could arrange this, if desired, although it required
    // more coordination between the client and server.

    tmc_udn_send_5(bme_server, UDN0_DEMUX_TAG,
                   EX_MSG_PROCESS,
                   my_hdr.word,
                   0,
                   len,
                   PROCESSING_BUFSIZE);

    reply = udn0_receive();
    if (reply != len)
    {
      fprintf(stderr, "client: got bad response %d to PROCESS "
              "message (expected %d)\n", reply, len);
      exit(1);
    }

    if (write(STDOUT_FILENO, outbuf, len) != len)
    {
      perror("write");
      exit(1);
    }
  }

  return 0;
}
