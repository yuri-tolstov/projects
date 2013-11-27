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
#define H2T_GENERATE_DATA_PATTERN
#endif

// By default, the host ring buffer is 4MB in size and it consists of
// 1024 entries of 4KB packet buffers. Modify GXPCI_HOST_PQ_SEGMENT_ENTRIES
// in <asm/tilegxpci.h> to change the number of entries in the ring.
// For detailed ring buffer configuration information, refer to the
// comments in <asm/tilegxpci.h>.
#define RING_BUF_ELEMS             GXPCI_HOST_PQ_SEGMENT_ENTRIES
#define RING_BUF_ELEM_SIZE         (HOST_PQ_SEGMENT_MAX_SIZE / \
                                    GXPCI_HOST_PQ_SEGMENT_ENTRIES)

#define likely(cond) __builtin_expect(!!(cond), 1)
#define unlikely(cond) __builtin_expect(!!(cond), 0)

// PCIe packet queue data for accessing packet queue data.
typedef struct {
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

// Minimum packets-per-second, as measured by host.
double host_min_pps = 0.0;

#ifdef GET_HOST_CPU_UTILIZATION
struct timeval busy_start;
struct timeval busy_end;
double busy_duration = 0;
int start_clk = 0;
#endif

/*********************************************************************/
/*                        Host-Side Packet Consumer                  */
/*********************************************************************/

#define TAIL_UPDATE_LIMIT_ENABLE

int h2t_pq_init(int queue_index, host_packet_queue_t* pq_info)
{
  char dev_name[40];

  unsigned int host_ring_buffer_size =
    host_ring_num_elems * host_buf_size;

  // Open the packet queue file.
  snprintf(dev_name, sizeof(dev_name), "/dev/tilegxpci%d/packet_queue/h2t/%d",
           card_index, queue_index);
  int pq_fd = open(dev_name, O_RDWR);
  if (pq_fd < 0)
  {
    fprintf(stderr, "Host: Failed to open '%s': %s\n", dev_name,
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

  // Configure and allocate the ring buffer for the transmit queue.
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

  // On the host side, mmap the transmmit queue region.
  pq_info->buffer =
    mmap(0, host_ring_buffer_size, PROT_READ | PROT_WRITE,
         MAP_SHARED, pq_fd, TILEPCI_PACKET_QUEUE_BUF_MMAP_OFFSET);
  assert(pq_info->buffer != MAP_FAILED);

  // On the host side, mmap the queue status.
  pq_info->pq_status=
    mmap(0, sizeof(struct tlr_pq_status), PROT_READ | PROT_WRITE,
         MAP_SHARED, pq_fd, TILEPCI_PACKET_QUEUE_STS_MMAP_OFFSET);
  assert(pq_info->pq_status != MAP_FAILED);

  pq_info->pq_regs->producer_index = 0;

  return 0;
}

// Send data from Host to Tile over PCIe packet queue. Queue must have
// already been opened by h2t_pq_init().
int h2t_pq_test(host_packet_queue_t* pq_info, unsigned long packets)
{
#ifdef H2T_GENERATE_DATA_PATTERN
  uint32_t data_pattern = 0;
  int j;
  uint32_t* ptrn_ptr;
#endif

  // Loop for some number of packets:
  unsigned long packet_count = 0;
  struct timeval start;
  struct timeval end;
  int secs, usecs;
  double duration, packets_per_sec, gigabits_per_sec;

  volatile uint32_t* consumer_index = &(pq_info->pq_regs->consumer_index);
  volatile uint32_t* producer_index = &(pq_info->pq_regs->producer_index);
  volatile enum gxpci_chan_status_t* status = &(pq_info->pq_status->status);
  uint32_t read;
  uint32_t write = 0;

  while (1)
  {
    if (unlikely(*status == GXPCI_CHAN_RESET))
    {
      printf("Host: H2T: channel is reset\n");
      goto done;
    }

    // Determine the number of data buffers that have been consumed
    // by the Tile application.
    read = *consumer_index;

#ifdef GET_HOST_CPU_UTILIZATION
    start_clk = 0;
    if (host_ring_num_elems - (write - read))
    {
      start_clk = 1;
      gettimeofday(&busy_start, NULL);
    }
#endif

    while (likely(host_ring_num_elems - (write - read)))
    {
      if (unlikely(*status == GXPCI_CHAN_RESET))
      {
        printf("Host: H2T: channel is reset\n");
        goto done;
      }

#ifdef H2T_GENERATE_DATA_PATTERN
      ptrn_ptr = (uint32_t*) (pq_info->buffer + 
        (write & (RING_BUF_ELEMS - 1)) * host_buf_size);
      for (j = 0; j < (packet_size >> 2); j++) 
      {
        ptrn_ptr[j] = data_pattern++;
      }
#endif
      if (unlikely(packet_count == 0))
      {
        gettimeofday(&start, NULL);
      }

      write++;

      // Update the write index register to inform the Tile side that the
      // packet has been written.
#ifdef TAIL_UPDATE_LIMIT_ENABLE
      if (unlikely((packet_count & 0x3f) == 0))
      {
        *producer_index = write;
      }
#else
      *producer_index = write;
#endif

#if DEBUG
      if (packet_count > 0 && packet_count % 1000000 == 0)
        printf("packet_count is now %lu\n", packet_count);
#endif

      packet_count++;
      if (unlikely(packet_count == packets))
      {
        goto done;
      }
    }
#ifdef GET_HOST_CPU_UTILIZATION
    if (start_clk)
    {
      gettimeofday(&busy_end, NULL);

      secs = busy_end.tv_sec - busy_start.tv_sec;
      usecs = busy_end.tv_usec - busy_start.tv_usec;

      while (usecs < 0)
      {
        usecs += 1000000;
        secs--;
      }
      busy_duration += (double) secs + ((double) usecs) / 1000000;
    }
#endif
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

#ifdef GET_HOST_CPU_UTILIZATION
  printf("Host: Sent %lu %d-byte packets, taking %fs (real %fs), %.3fgbps, "
         "%.3fmpps\n", packet_count, packet_size, duration, busy_duration,
         gigabits_per_sec / 1e9, packets_per_sec / 1e6);
#else
  printf("Host: Sent %lu %d-byte packets, taking %fs, %.3fgbps, %.3fmpps\n",
         packet_count, packet_size, duration, gigabits_per_sec / 1e9,
         packets_per_sec / 1e6);
#endif

  // Fail if we didn't reach the minimum required performance.
  return (packets_per_sec < host_min_pps);
}

int t2h_pq_init(int queue_index, host_packet_queue_t* pq_info)
{
  char dev_name[40];

  unsigned int host_ring_buffer_size =
    host_ring_num_elems * host_buf_size;

  // Open the packet queue file.
  snprintf(dev_name, sizeof(dev_name), "/dev/tilegxpci%d/packet_queue/t2h/%d",
           card_index, queue_index);
  int pq_fd = open(dev_name, O_RDWR);
  if (pq_fd < 0)
  {
    fprintf(stderr, "Host: Failed to open '%s': %s\n", dev_name,
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

  // Configure and allocate the ring buffer for the receive queue.
  tilepci_packet_queue_info_t buf_info;

  buf_info.buf_size = host_buf_size;

  int err = ioctl(pq_fd, TILEPCI_IOC_SET_PACKET_QUEUE_BUF, &buf_info);
  if (err < 0)
  {
    fprintf(stderr, "Host: Failed TILEPCI_IOC_SET_PACKET_QUEUE_BUF: %s\n",
            strerror(errno));
    return -1;
  }

  // On the host side, mmap the receive queue region.
  pq_info->buffer =
    mmap(0, host_ring_buffer_size, PROT_READ | PROT_WRITE,
         MAP_SHARED, pq_fd, TILEPCI_PACKET_QUEUE_BUF_MMAP_OFFSET);
  assert(pq_info->buffer != MAP_FAILED);

  // On the host side, mmap the queue status.
  pq_info->pq_status =
    mmap(0, sizeof(struct tlr_pq_status), PROT_READ | PROT_WRITE,
         MAP_SHARED, pq_fd, TILEPCI_PACKET_QUEUE_STS_MMAP_OFFSET);
  assert(pq_info->pq_status != MAP_FAILED);

  pq_info->pq_regs->consumer_index = 0;

  return 0;
}

int t2h_pq_test(host_packet_queue_t* pq_info, unsigned long packets)
{
#ifdef T2H_CHECK_DATA_PATTERN
  void* buffer_ptr;
  uint32_t actual_data;
  uint32_t expected_data = 0;
  int data_fail = 0;
  int j;
#endif

  // Loop for some number of packets:
  unsigned long packet_count = 0;
  struct timeval start;
  struct timeval end;
  int secs, usecs;
  double duration, packets_per_sec, gigabits_per_sec;

  volatile uint32_t* producer_index = &(pq_info->pq_regs->producer_index);

  volatile uint32_t* consumer_index = &(pq_info->pq_regs->consumer_index);
  volatile enum gxpci_chan_status_t* status = &(pq_info->pq_status->status);
  uint32_t write;
  uint32_t read = 0;

  while (1)
  {
    if (unlikely(*status == GXPCI_CHAN_RESET))
    {
      printf("Host: T2H: channel is reset\n");
      goto done;
    }

    // Get packets off the ring buffer by accessing the receive queue at
    // the new write index.
    write = *producer_index;

#ifdef GET_HOST_CPU_UTILIZATION
    start_clk = 0;
    if (host_ring_num_elems - (write - read))
    {
      start_clk = 1;
      gettimeofday(&busy_start, NULL);
    }
#endif

    while (likely(write != read))
    {
      if (unlikely(*status == GXPCI_CHAN_RESET))
      {
        printf("Host: T2H: channel is reset\n");
        goto done;
      }

      if (unlikely(packet_count == 0))
      {
        gettimeofday(&start, NULL);
      }

#ifdef T2H_CHECK_DATA_PATTERN
      buffer_ptr = pq_info->buffer + 
	((read & (RING_BUF_ELEMS - 1)) * host_buf_size);
      for (j = 0; j < packet_size; j += 4)
      {
        actual_data = *(uint32_t*) buffer_ptr;
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

      read++;

      // Update the read index register to inform the Tile side that the
      // packet has been read.
#if defined(TAIL_UPDATE_LIMIT_ENABLE)
      if (unlikely((packet_count & 0x3f) == 0))
      {
        *consumer_index = read;
      }
#else
      *consumer_index = read;
#endif

#if DEBUG
      if (packet_count > 0 && packet_count % 1000000 == 0)
        printf("packet_count is now %lu\n", packet_count);
#endif

      packet_count++;
      if (unlikely(packet_count == packets))
      {
        goto done;
      }
    }
#ifdef GET_HOST_CPU_UTILIZATION
    if (start_clk)
    {
      gettimeofday(&busy_end, NULL);

      secs = busy_end.tv_sec - busy_start.tv_sec;
      usecs = busy_end.tv_usec - busy_start.tv_usec;

      while (usecs < 0)
      {
        usecs += 1000000;
        secs--;
      }
      busy_duration += (double) secs + ((double) usecs) / 1000000;
    }
#endif
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

#ifdef GET_HOST_CPU_UTILIZATION
  printf("Host: Received %lu %d-byte packets, taking %fs (real %fs), %.3fgbps, "
         "%.3fmpps\n", packet_count, packet_size, duration, busy_duration,
         gigabits_per_sec / 1e9, packets_per_sec / 1e6);
#else
  printf("Host: Received %lu %d-byte packets, taking %fs, %.3fgbps, %.3fmpps\n",
         packet_count, packet_size, duration, gigabits_per_sec / 1e9,
         packets_per_sec / 1e6);
#endif

#ifdef T2H_CHECK_DATA_PATTERN
  printf("Host: Data checked for %u words (mod 4G)\n", expected_data);
#endif

  // Fail if we didn't reach the minimum required performance.
  return (packets_per_sec < host_min_pps);
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
    else if ((opt = shift_option(&args, "--host_min_pps=")))
      host_min_pps = strtod(opt, NULL);
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

  host_packet_queue_t pq_info;
#ifdef TEST_T2H
  if (t2h_pq_init(queue_index, &pq_info)) 
    abort();
  return t2h_pq_test(&pq_info, packets);
#else
  if (h2t_pq_init(queue_index, &pq_info)) 
    abort();
  return h2t_pq_test(&pq_info, packets);
#endif 
}
