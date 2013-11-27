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


// The zero-copy queue index, with valid values from 0 to 7.
static int queue_idx = 0;

static size_t pkt_size = 4096;

// The "/dev/tilegxpci%d" device to be used.
unsigned int card_index = 0;

static char *
shift_option(char ***arglist, const char* option)
{
  char** args = *arglist;
  char* arg = args[0], **rest = &args[1];
  int optlen = strlen(option);
  char* val = arg + optlen;
  if (option[optlen - 1] != '=')
  {
    if (strcmp(arg, option))
      return NULL;
  }
  else
  {
    if (strncmp(arg, option, optlen - 1))
      return NULL;
    if (arg[optlen - 1] == '\0')
      val = *rest++;
    else if (arg[optlen - 1] != '=')
      return NULL;
  }
  *arglist = rest;
  return val;
}


// Parses command line arguments in order to fill in the queue index
// and packet size variables.
static void
parse_args(int argc, char** argv)
{
  char **args = &argv[1];

  // Scan options.
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--queue=")))
    {
      queue_idx = atoi(opt);
      if (queue_idx < 0 || queue_idx >= TILEPCI_NUM_ZC_H2T_CHAN)
      {
        queue_idx = 0;
        fprintf(stderr,
                "Illegal zero-copy queue index, use default queue 0 instead\n");
      }
    }
    else if ((opt = shift_option(&args, "--size=")))
    {
      pkt_size = atoi(opt);
    }
    else if ((opt = shift_option(&args, "--card=")))
    {
      card_index = atoi(opt);
    }
    else
    {
      fprintf(stderr, "Unknown option '%s'.\n", args[0]);
#ifdef __tile__
      fprintf(stderr, "Usage: simple.tile --queue=0 --size=4096\n");
#else
      fprintf(stderr, "Usage: ./simple.host --queue=0 --size=4096 --card=0\n");
#endif // __tile__
      exit(EXIT_FAILURE);
    }
  }
}


#ifdef __tile__

int 
run_test(int queue_index, size_t pkt_size)
{
  // Open the tile side of a host-to-tile zero-copy channel.
  const char* path = "/dev/trio0-mac0/h2t/%d";
  char name[128];
  sprintf(name, path, queue_index); 

  int fd = open(name, O_RDWR);
  if (fd < 0)
  {
    fprintf(stderr, "Failed to open '%s': %s\n", name, strerror(errno));
    abort();
  }

  // On the tile side, packet buffers can come from any memory region,
  // but they must not cross a page boundary.
  void *buffer = memalign(getpagesize(), pkt_size);
  assert(buffer != NULL);

  // Register this buffer to TRIO.
  tilegxpci_buf_info_t buf_info = {
    .va = (uintptr_t) buffer,
    .size = getpagesize(),
  };

  int result = ioctl(fd, TILEPCI_IOC_REG_BUF, &buf_info);
  assert(result == 0);

  // Given that we're the tile side of a host-to-tile channel, we can
  // construct a receive command and post it to the PCI subsystem.
  tilepci_xfer_req_t recv_cmd = {
    .addr = (uintptr_t) buffer,
    .len = pkt_size,
    .cookie = 0,
  };

  // Initiate the command.
  while (write(fd, &recv_cmd, sizeof(recv_cmd)) != sizeof(recv_cmd))
    fprintf(stderr, "write() failed: %s\n", strerror(errno));

  // Read back the completion.
  tilepci_xfer_comp_t comp;
  while (read(fd, &comp, sizeof(comp)) != sizeof(comp))
    fprintf(stderr, "read() failed: %s\n", strerror(errno));

  // Print the received data, including the metadata 'cookie' that
  // came along with the data.
  printf("Received %d bytes: cookie = %#lx, words[0] = %#x\n",
         comp.len, (unsigned long)comp.cookie, 
	 ((uint32_t*)(uintptr_t)comp.addr)[0]);

  return 0;
}

#else

int
run_test(int queue_index, size_t pkt_size)
{
  // Open the host side of a host-to-tile zero-copy channel.
  const char* path = "/dev/tilegxpci%d/h2t/%d";
  char name[128];
  sprintf(name, path, card_index, queue_index);

  int fd = open(name, O_RDWR);
  if (fd < 0)
  {
    fprintf(stderr, "Failed to open '%s': %s\n", path, strerror(errno));
    abort();
  }

  // On the host side, packet buffers must come from the per-channel
  // 'fast memory' region.
  void* buffer = mmap(0, pkt_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
  assert(buffer != MAP_FAILED);

  ((uint32_t*)buffer)[0] = 0x10002000;

  // Given that we're the host side of a host-to-tile channel, we can
  // construct a send command and post it to the PCI subsystem.
  tilepci_xfer_req_t send_cmd = {
    .addr = (uintptr_t) buffer,
    .len = pkt_size,
    .cookie = 0x12340000,
  };

  // Initiate the command.
  while (write(fd, &send_cmd, sizeof(send_cmd)) != sizeof(send_cmd))
    fprintf(stderr, "write() failed: %s\n", strerror(errno));
  
  // Read back the completion.
  tilepci_xfer_comp_t comp;
  while (read(fd, &comp, sizeof(comp)) != sizeof(comp))
    fprintf(stderr, "read() failed: %s\n", strerror(errno));

  // Print the send data, including the metadata 'cookie' that
  // came along with the data.
  printf("Sent %d bytes: cookie = %#lx, words[0] = %#x\n",
         comp.len, (unsigned long)comp.cookie, 
         ((uint32_t*)(uintptr_t)comp.addr)[0]);

  return 0;
}

#endif // __tile__


int 
main(int argc, char** argv)
{
  parse_args(argc, argv);

  return run_test(queue_idx, pkt_size);
}
