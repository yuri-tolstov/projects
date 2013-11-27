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

#if 0
#define T2H_CHECK_DATA_PATTERN
#endif

// By default, the host ring buffer is 4MB in size and it consists of
// 1024 entries of 4KB packet buffers. Modify GXPCI_HOST_PQ_SEGMENT_ENTRIES
// in <asm/tilegxpci.h> to change the size of each ring. To increase
// the total ring buffer size, re-configure the host kernel using
// CONFIG_FORCE_MAX_ZONEORDER.
#define RING_BUF_ELEMS             GXPCI_HOST_PQ_SEGMENT_ENTRIES
#define RING_BUF_ELEM_SIZE         (HOST_PQ_SEGMENT_MAX_SIZE / \
                                    GXPCI_HOST_PQ_SEGMENT_ENTRIES)

// PCIe packet queue data for accessing packet queue data.
typedef struct {
  char dev_name[40];
  struct tlr_pq_status* pq_status;
  struct gxpci_host_pq_regs_app* pq_regs;
  void* buffer;
  int pq_fd;
} host_packet_queue_t;

// The "/dev/tilegxpci%d" device to be used.
unsigned int card_index = 0;

// Size of packet from the sender.
unsigned int packet_size = PACKET_SIZE;

// Number of buffers in the host ring buffer.
static const unsigned int  host_ring_num_elems = RING_BUF_ELEMS;

// Size of each buffer in the Tile-to-host bring buffer.
static unsigned int  host_buf_size = RING_BUF_ELEM_SIZE;


/*********************************************************************/
/*                        Host-Side Packet Consumer                  */
/*********************************************************************/

#define TAIL_UPDATE_LIMIT_ENABLE

int pq_init(host_packet_queue_t* pq_info)
{
  unsigned int host_ring_buffer_size = host_ring_num_elems * host_buf_size;

  // Open the packet queue file.
  int pq_fd = open(pq_info->dev_name, O_RDWR);
  if (pq_fd < 0)
  {
    fprintf(stderr, "Host: Failed to open '%s': %s\n", pq_info->dev_name,
            strerror(errno));
    return -1;
  }

  // mmap the register space.
  pq_info->pq_regs =
    (struct gxpci_host_pq_regs_app*)
    mmap(0, sizeof(struct gxpci_host_pq_regs_app),
         PROT_READ | PROT_WRITE,
         MAP_SHARED, pq_fd, TILEPCI_PACKET_QUEUE_INDICES_MMAP_OFFSET);
  assert(pq_info->pq_regs != MAP_FAILED);

  // Configure and allocate the DMA ring buffer.
  tilepci_packet_queue_info_t buf_info;

  // buf_size for h2t is used to pre-allocate the buffer pointers on the Tile 
  // side.
  buf_info.buf_size = host_buf_size;

  int err = ioctl(pq_fd, TILEPCI_IOC_SET_PACKET_QUEUE_BUF, &buf_info);
  if (err < 0)
  {
    fprintf(stderr, "Host: Failed TILEPCI_IOC_SET_PACKET_QUEUE_BUF: %s\n",
            strerror(errno));
    return -2;
  }

  // On the host side, mmap the DMA ring buffer.
  pq_info->buffer =
    mmap(0, host_ring_buffer_size, PROT_READ | PROT_WRITE,
         MAP_SHARED, pq_fd, TILEPCI_PACKET_QUEUE_BUF_MMAP_OFFSET);
  assert(pq_info->buffer != MAP_FAILED);

  // On the host side, mmap the queue status.
  pq_info->pq_status=
    mmap(0, sizeof(struct tlr_pq_status), PROT_READ | PROT_WRITE,
         MAP_SHARED, pq_fd, TILEPCI_PACKET_QUEUE_STS_MMAP_OFFSET);
  assert(pq_info->pq_status != MAP_FAILED);

  return 0;
}

void pq_loopback_test(host_packet_queue_t* pq_h2t_info,
                      host_packet_queue_t* pq_t2h_info,
                      unsigned long packets)
{
  // Loop for some number of packets:
  unsigned long packet_count = 0;
  double duration, packets_per_sec, gigabits_per_sec;
  struct timeval start;
  struct timeval end;
  int secs, usecs;

  volatile enum gxpci_chan_status_t* h2t_status =
    &(pq_h2t_info->pq_status->status);
  volatile enum gxpci_chan_status_t* t2h_status =
    &(pq_t2h_info->pq_status->status);

  volatile uint32_t* h2t_consumer_index =
    &(pq_h2t_info->pq_regs->consumer_index);
  volatile uint32_t* t2h_producer_index =
    &(pq_t2h_info->pq_regs->producer_index);

  volatile uint32_t* h2t_producer_index =
    &(pq_h2t_info->pq_regs->producer_index);
  volatile uint32_t* t2h_consumer_index =
    &(pq_t2h_info->pq_regs->consumer_index);

  uint32_t h2t_write = 0, t2h_read = 0;
  uint32_t h2t_read;
  uint32_t t2h_write;

#ifdef T2H_CHECK_DATA_PATTERN
  void* buffer_ptr;
  uint32_t expected_data = 0, actual_data;
  int data_fail = 0, j;
#endif

  while (1)
  {
    if (*h2t_status == GXPCI_CHAN_RESET || *t2h_status == GXPCI_CHAN_RESET)
    {
      printf("Host: channel is reset\n");
      goto done;
    }

    // First step, get packets from Tile via the T2H Packet Queue.
    //
    // Get packets off the ring buffer by accessing the receive queue at
    // the new write index.
    t2h_write = *t2h_producer_index;

    // Determine the number of packets that have been looped back to the TILE-Gx
    // chip and use the count to update the T2H Packet Queue's consumer index.
    // This ensures that the T2H Packet Queue doesn't overwrite host buffers
    // that have not been written back to the TILE-Gx chip yet. 
    h2t_read = *h2t_consumer_index;
    if (t2h_read != h2t_read)
      *t2h_consumer_index = h2t_read;

    while (t2h_write != t2h_read)
    {
      if (*h2t_status == GXPCI_CHAN_RESET || *t2h_status == GXPCI_CHAN_RESET)
      {
        printf("Host: channel is reset\n");
        goto done;
      }

      if (packet_count == 0)
      {
        gettimeofday(&start, NULL);
      }

#ifdef T2H_CHECK_DATA_PATTERN
      buffer_ptr = pq_t2h_info->buffer + 
	((t2h_read & (host_ring_num_elems - 1)) * host_buf_size);
      for (j = 0; j < packet_size; j += 4)
      {
        actual_data = * ((uint32_t*) buffer_ptr);
        if (actual_data != expected_data) 
        {
          printf("Host: T2H: data error. Expected 0x%08x but received 0x%08x\n", 
                 expected_data, actual_data);
          data_fail = 1;
        }
        expected_data++;
        buffer_ptr += 4;
      }      
      if (data_fail)
        goto done;
#endif

      // Second step, send packets back to Tile via the H2T Packet Queue.
      //
      // Note that the H2T and T2H Packet Queues are required to share a same
      // DMA ring buffer by adding kernel module parameters "pq_h2t_mems=,,1
      // pq_t2h_mems=,,1" when inserting tilegxpci.ko module, all the packets
      // received via the T2H Packet Queue are looping back to Tile via the H2T
      // Packet Queue.
      h2t_write++;

      // Update the write index register to inform the Tile side that the
      // packet has been written.
#ifdef TAIL_UPDATE_LIMIT_ENABLE
      if ((packet_count & 0x3f) == 0)
      {
        *h2t_producer_index = h2t_write;
      }
#else
      *h2t_producer_index = h2t_write;
#endif

      t2h_read++;

      // Count the loopback packets.
      packet_count++;
      if (packet_count == packets)
      {
        // Try to update the H2T Packet Queue's producer index if it has not
        // been done yet.
        *h2t_producer_index = h2t_write;

        // Wait until the TILE-Gx chip consumes all the packets.
        while (1)
        {
          h2t_read = *h2t_consumer_index;
          if (h2t_read == h2t_write)
            break;
        }

        // Try to update the T2H Packet Queue's consumer index if it has not
        // been done yet.
        if (h2t_read != *t2h_consumer_index)
          *t2h_consumer_index = h2t_read;

        goto done;
      }
    }
  }

done:

  gettimeofday(&end, NULL);

  secs = end.tv_sec - start.tv_sec;
  usecs = end.tv_usec - start.tv_usec;

  while (usecs < 0)
  {
    usecs += 1000000;
    secs--;
  }
  duration = (double) secs + ((double) usecs) / 1000000;
  packets_per_sec = ((double) packet_count) / duration;
  gigabits_per_sec = packets_per_sec * packet_size * 8;

  printf("Host: Loop %lu %d-byte packets, taking %fs, %.3fgbps, %.3fmpps\n",
         packet_count, packet_size, duration, gigabits_per_sec / 1e9,
         packets_per_sec / 1e6);
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
char *shift_option(char*** arglist, const char* option)
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
  char** args = &argv[1];

  // Packet queue index.
  unsigned int queue_index = 0;

  // Number of packets sent.
  unsigned long packets = 125000000;

  // Scan options.
  while (*args)
  {
    char* opt = NULL;

    if ((opt = shift_option(&args, "--packet_size=")))
      packet_size = strtoul(opt, NULL, 0);
    else if ((opt = shift_option(&args, "--packets=")))
      packets = strtoul(opt, NULL, 0);
    else if ((opt = shift_option(&args, "--queue_index=")))
      queue_index = strtoul(opt, NULL, 0);
    else if ((opt = shift_option(&args, "--host_buf_size=")))
      host_buf_size = strtoul(opt, NULL, 0);
    else if ((opt = shift_option(&args, "--card=")))
      card_index = strtoul(opt, NULL, 0);
    else
    {
      fprintf(stderr, "Host: Unknown option '%s'.\n", args[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (host_buf_size < packet_size)
  {
    fprintf(stderr, "Host: host_buf_size must be >= packet_size.\n");
    exit(EXIT_FAILURE);
  }

  host_packet_queue_t pq_h2t_info;
  host_packet_queue_t pq_t2h_info;

  snprintf(pq_h2t_info.dev_name, sizeof(pq_h2t_info.dev_name),
           "/dev/tilegxpci%d/packet_queue/h2t/%d",
           card_index, queue_index);
  snprintf(pq_t2h_info.dev_name, sizeof(pq_t2h_info.dev_name),
           "/dev/tilegxpci%d/packet_queue/t2h/%d",
           card_index, queue_index);

  if (pq_init(&pq_h2t_info)) 
    abort();
  pq_h2t_info.pq_regs->producer_index = 0;

  if (pq_init(&pq_t2h_info)) 
    abort();
  pq_t2h_info.pq_regs->consumer_index = 0;

  pq_loopback_test(&pq_h2t_info, &pq_t2h_info, packets);
}
