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
// Host program that demostrates the Gx PCIe Packet Queue API.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#include <asm/tilegxpci.h>

#if 0
#define PACKET_SIZE                (4 * 1024)
#else
#define PACKET_SIZE                64
#endif

// Size of packet from the sender.
unsigned int packet_size = PACKET_SIZE;

// Packet queue index.
unsigned int queue_index;

// Size of the host Raw DMA buffer, reserved by the kernel boot argument.
unsigned int  host_buf_size;

#ifdef RAW_DMA_USE_RESERVED_MEMORY

// Physical address of the host Raw DMA buffer, to be mapped into user-space.
unsigned long host_buf_addr;

#endif

// The "/dev/tilegxpci%d" device to be used.
unsigned int card_index = 0;

/*********************************************************************/
/*                        Host-Side Packet Consumer                  */
/*********************************************************************/

int run_h2t_test(int argc, char** argv)
{
  char dev_name[40];

  // Open the Raw DMA file.
  snprintf(dev_name, sizeof(dev_name), "/dev/tilegxpci%d/raw_dma/h2t/%d",
           card_index, queue_index);
  int rd_fd = open(dev_name, O_RDWR);
  if (rd_fd < 0)
  {
    fprintf(stderr, "Host: Failed to open '%s': %s\n", dev_name,
            strerror(errno));
    abort();
  }

  // Obtain the reserved DMA buffer's size and address.
  tilepci_raw_dma_get_buf_t buf_info;

  int err = ioctl(rd_fd, TILEPCI_IOC_GET_RAW_DMA_BUF, &buf_info);
  if (err < 0)
  {
    fprintf(stderr, "Host: Failed TILEPCI_IOC_GET_RAW_DMA_BUF: %s\n",
            strerror(errno));
    abort();
  }

  host_buf_size = buf_info.rd_buf_size;

#ifdef RAW_DMA_USE_RESERVED_MEMORY

  host_buf_addr = buf_info.rd_buf_bus_addr;

  // On the host side, mmap the transmit DMA buffer *reserved* by the kernel.
  int mem_fd = open("/dev/mem", O_RDWR);
  void* buffer = mmap(0, host_buf_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, mem_fd, host_buf_addr);
  assert(buffer != MAP_FAILED);

  printf("Host transmit buffer %#x@%p, mapped at %p\n", host_buf_size,
         (void *)host_buf_addr, buffer);

  // Zero out the first 64 bytes, which may contain
  // application-specific flow-control information.
  memset(buffer, 0, 64);

#else

  // On the host side, mmap the transmit DMA buffer *allocated* by the driver.
  void* buffer = mmap(0, host_buf_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, rd_fd, TILEPCI_RAW_DMA_BUF_MMAP_OFFSET);
  assert(buffer != MAP_FAILED);

#endif

  // On the host side, mmap the queue status.
  struct tlr_rd_status *rd_status=
    mmap(0, sizeof(struct tlr_rd_status), PROT_READ | PROT_WRITE,
         MAP_SHARED, rd_fd, TILEPCI_RAW_DMA_STS_MMAP_OFFSET);
  assert(rd_status != MAP_FAILED);

  // mmap the FC register, e.g. the producer index for H2T queue.
  struct gxpci_host_rd_regs_app *rd_regs =
    mmap(0, sizeof(struct gxpci_host_rd_regs_app), PROT_READ | PROT_WRITE,
         MAP_SHARED, rd_fd, TILEPCI_RAW_DMA_REG_MMAP_OFFSET);
  assert(rd_regs != MAP_FAILED);

  // Activate this transmit queue.
  err = ioctl(rd_fd, TILEPCI_IOC_ACTIVATE_RAW_DMA, NULL);
  if (err < 0)
  {
    fprintf(stderr, "Host: Failed TILEPCI_IOC_ACTIVATE_RAW_DMA: %s\n",
            strerror(errno));
    abort();
  }

  while (1)
  {
    if (rd_status->status == GXPCI_CHAN_RESET)
    {
      printf("Host: H2T: channel is reset\n");
      break;
    }
    sleep(1);
  }

  return 0;
}

int run_t2h_test(int argc, char** argv)
{
  char dev_name[40];

  // Open the packet queue file.
  snprintf(dev_name, sizeof(dev_name), "/dev/tilegxpci%d/raw_dma/t2h/%d",
           card_index, queue_index);
  int rd_fd = open(dev_name, O_RDWR);
  if (rd_fd < 0)
  {
    fprintf(stderr, "Host: Failed to open '%s': %s\n", dev_name,
            strerror(errno));
    abort();
  }

  // Obtain the reserved DMA buffer's size and address.
  tilepci_raw_dma_get_buf_t buf_info;

  int err = ioctl(rd_fd, TILEPCI_IOC_GET_RAW_DMA_BUF, &buf_info);
  if (err < 0)
  {
    fprintf(stderr, "Host: Failed TILEPCI_IOC_GET_RAW_DMA_BUF: %s\n",
            strerror(errno));
    abort();
  }

  host_buf_size = buf_info.rd_buf_size;

#ifdef RAW_DMA_USE_RESERVED_MEMORY

  host_buf_addr = buf_info.rd_buf_bus_addr;

  // On the host side, mmap the receive DMA buffer *reserved* by the kernel.
  int mem_fd = open("/dev/mem", O_RDWR);
  void* buffer = mmap(0, host_buf_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, mem_fd, host_buf_addr);
  assert(buffer != MAP_FAILED);

  printf("Host Receive buffer %#x@%p, mapped at %p\n", host_buf_size,
         (void *)host_buf_addr, buffer);

  // Zero out the first 64 bytes.
  memset(buffer, 0, 64);

#else

  // On the host side, mmap the receive DMA buffer *allocated* by the driver.
  void* buffer = mmap(0, host_buf_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, rd_fd, TILEPCI_RAW_DMA_BUF_MMAP_OFFSET);
  assert(buffer != MAP_FAILED);

#endif

  // On the host side, mmap the queue status.
  struct tlr_rd_status *rd_status=
    mmap(0, sizeof(struct tlr_rd_status), PROT_READ | PROT_WRITE,
         MAP_SHARED, rd_fd, TILEPCI_RAW_DMA_STS_MMAP_OFFSET);
  assert(rd_status != MAP_FAILED);

  // mmap the FC register, e.g. the consumer index for T2H queue.
  struct gxpci_host_rd_regs_app *rd_regs =
    mmap(0, sizeof(struct gxpci_host_rd_regs_app), PROT_READ | PROT_WRITE,
         MAP_SHARED, rd_fd, TILEPCI_RAW_DMA_REG_MMAP_OFFSET);
  assert(rd_regs != MAP_FAILED);

  // Activate this receive queue.
  err = ioctl(rd_fd, TILEPCI_IOC_ACTIVATE_RAW_DMA, NULL);
  if (err < 0)
  {
    fprintf(stderr, "Host: Failed TILEPCI_IOC_ACTIVATE_RAW_DMA: %s\n",
            strerror(errno));
    abort();
  }

  while (1)
  {
    if (rd_status->status == GXPCI_CHAN_RESET)
    {
      printf("Host: T2H: channel is reset\n");
      break;
    }
    sleep(1);
  }
  
  return 0;
}

//----------------------------------------
// Match argument list option.
// Options ending in '=' take an additional value, which may be
// attached in the same argument or detached in the following
// argument.
// @param arglist       Points to remaining argv, updated on match.
// @param option        The option to match, ending in '=' if it takes a value.
// @return              Value if option matches, NULL otherwise.
//
char
*shift_option(char ***arglist, const char* option)
{
  char** args = *arglist;
  char* arg = args[0], **rest = &args[1];
  int optlen = strlen(option);
  char* val = arg+optlen;
  if (option[optlen - 1] != '=')
  {
    if (strcmp(arg, option))
      return NULL;
  }
  else
  {
    if (strncmp(arg, option, optlen-1))
      return NULL;
    if (arg[optlen- 1 ] == '\0')
      val = *rest++;
    else if (arg[optlen - 1] != '=')
      return NULL;
  }
  *arglist = rest;
  return val;
}

int main(int argc, char** argv)
{
  char **args = &argv[1];

  // Scan options.
  //
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--packet_size=")))
      packet_size = strtoul(opt, NULL, 0);
    else if ((opt = shift_option(&args, "--queue_index=")))
      queue_index = strtoul(opt, NULL, 0);
    else if ((opt = shift_option(&args, "--card=")))
      card_index = strtoul(opt, NULL, 0);
    else
    {
      fprintf(stderr, "Host: Unknown option '%s'.\n", args[0]);
      exit(EXIT_FAILURE);
    }
  }

#ifdef TEST_T2H
  return run_t2h_test(argc, argv);
#else
  return run_h2t_test(argc, argv);
#endif 
}
