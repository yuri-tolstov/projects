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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arch/sim.h>
#include <gxio/trio.h>
#include <gxpci/gxpci.h>
#include <tmc/cpus.h>
#include <tmc/alloc.h>
#include <tmc/task.h>


#define DATA_VALIDATION

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxpci_strerror(__val));       \
  } while (0)

// The packet pool memory size.
#define MAP_LENGTH 		(16 * 1024 * 1024)

#define PKT_BUF_STRIDE		16

// The number of packets that this program sends.
#define SEND_PKT_COUNT 		30

#if 1

// The size of a single packet.
#define SEND_PKT_SIZE 		(16 * 1024)

// The size of receive buffers posted by the receiver.
#define RECV_BUFFER_SIZE 	(16 * 1024)

#else

#define SEND_PKT_SIZE           (1024)

#define RECV_BUFFER_SIZE        (1024)

#endif

#if 1

// The size of space that the receiver wants to preserve
// at the beginning of the packet, e.g. for packet header
// that is to be filled after the packet is received.
#define RECV_PKT_HEADROOM	14

#else

#define RECV_PKT_HEADROOM	0

#endif

//
// These are the spaces leading and trailing the packet
// that are inspected to validate DMA correctness.
//
#if 1
#define PKT_CLEAR_SPACE		16
#else
#define PKT_CLEAR_SPACE		0
#endif

// We run two copies of this program, one on each peered simulator.
// We use a different TRIO MAC and bus address for each simulator.
#define SIM0_MAC 		0
#define SIM1_MAC 		1

#define SIM0_MAC_INDEX 		1
#define SIM1_MAC_INDEX 		2

// The queue index of a C2C queue, used by both sender and receiver.
#define QUEUE_INDEX 		3

// The TRIO index.
int trio_index = 0;

// The MAC and bus addresses for this simulator.
int g_mac;

// Parses command line arguments in order to fill in the MAC and bus
// address variables.
void parse_args(int argc, char** argv)
{
  if (argc < 2)
    tmc_task_die("Usage: c2c_send sim_number");
  int sim = atoi(argv[1]);
  
  if (sim == 0)
  {
    g_mac = SIM0_MAC;
  }
  else
  {
    g_mac = SIM1_MAC;
  }
  printf("mac=%d\n", g_mac);
}

void do_send(gxpci_context_t* context, void* buf_mem)
{
  int result;
  int completions = 0;
  int sends = 0;

  gxpci_cmd_t cmd = {
    .buffer = buf_mem,
    .size = SEND_PKT_SIZE,
  };

#ifdef DATA_VALIDATION
  //
  // Before issuing bulk packets, send one packet for DMA data verification.
  // We also verify the adjacent area are not corrupted.
  //
  char* source = (char*)buf_mem;
  for (int i = 0; i < SEND_PKT_SIZE; i++)
    source[i] = i + (i >> 8) + 1;
  __insn_mf();

  do {
    result = gxpci_post_cmd(context, &cmd);
  } while (result == GXPCI_ECREDITS);
  VERIFY_ZERO(result, "gxpci_post_cmd()");

  printf("Sent validation packet, size: %d\n", cmd.size);

  gxpci_comp_t comp;
  result = gxpci_get_comps(context, &comp, 1, 1);
  if (result != 1)
    tmc_task_die("Failure in gxpci_get_comps: %d.", result);

  printf("Completed validation packet, size: %d\n", comp.size);
#endif

  while (completions < SEND_PKT_COUNT)
  {
    if (sends < SEND_PKT_COUNT)
    {
      cmd.buffer = (char*)buf_mem + PKT_BUF_STRIDE *
        (sends & (MAP_LENGTH / PKT_BUF_STRIDE - 1));
      if ((cmd.buffer + cmd.size) > (buf_mem + MAP_LENGTH))
        cmd.buffer = (char*)buf_mem;
      result = gxpci_post_cmd(context, &cmd);
      if (result == 0)
        sends++;
    }

    gxpci_comp_t comp;
    result = gxpci_get_comps(context, &comp, 0, 1);
    if (result == 1)
    {
      printf("Send packet addr: %#lx size: %d\n",
        (unsigned long)comp.buffer, comp.size);
      completions++;
    }
  }
  printf("Sender finished %d cmds\n", completions);
}

void do_recv(gxpci_context_t* context, void* buf_mem)
{
  int result;
  int recv_pkt_count = 0;

  gxpci_cmd_t cmd = {
    .buffer = buf_mem,
    .size = RECV_BUFFER_SIZE,
  };

#ifdef DATA_VALIDATION
  int recv_pkt_size;

  if (RECV_PKT_HEADROOM + SEND_PKT_SIZE > RECV_BUFFER_SIZE)
    recv_pkt_size = RECV_BUFFER_SIZE - RECV_PKT_HEADROOM;
  else
    recv_pkt_size = SEND_PKT_SIZE;

  //
  // Receive one packet first to validate the DMA correctness.
  //
  char* source = (char*)buf_mem;
  for (int i = 0; i < (PKT_CLEAR_SPACE + RECV_PKT_HEADROOM); i++)
    source[i] = 0x55;
  source += PKT_CLEAR_SPACE + RECV_PKT_HEADROOM + recv_pkt_size;
  for (int i = 0; i < PKT_CLEAR_SPACE; i++)
    source[i] = 0x55;

  cmd.buffer = buf_mem + PKT_CLEAR_SPACE;
  result = gxpci_post_cmd(context, &cmd);
  VERIFY_ZERO(result, "gxpci_post_cmd()");

  gxpci_comp_t comp;
  result = gxpci_get_comps(context, &comp, 1, 1);
  if (result != 1)
    tmc_task_die("Failure in gxpci_get_comps: %d.", result);

  // Check results.
  source = (char*)buf_mem;
  for (int i = 0; i < (PKT_CLEAR_SPACE + RECV_PKT_HEADROOM); i++)
  {
    if (source[i] != 0x55)
      tmc_task_die("Leading data corruption at byte %d, %d.\n", i, source[i]);
  }

  source += PKT_CLEAR_SPACE + RECV_PKT_HEADROOM;
  for (int i = 0; i < recv_pkt_size; i++)
  {
    if (source[i] != (char)(i + (i >> 8) + 1))
      tmc_task_die("Data payload corruption at byte %d, %d.\n", i, source[i]);
  }

  source += recv_pkt_size;
  for (int i = 0; i < PKT_CLEAR_SPACE; i++)
  {
    if (source[i] != 0x55)
      tmc_task_die("Trailing data corruption at byte %d, %d.\n", i, source[i]);
  }

  printf("Verified data\n");
#endif

  while (recv_pkt_count < SEND_PKT_COUNT) {

    for (int i = 0; i < 64; i++)
    {
      cmd.buffer = buf_mem + 256 * i;
      result = gxpci_post_cmd(context, &cmd);
      if (result == GXPCI_ECREDITS)
        break;

      VERIFY_ZERO(result, "gxpci_post_cmd()");
    }

    for (int i = 0; i < 32; i++)
    {
      gxpci_comp_t comp;
      result = gxpci_get_comps(context, &comp, 0, 1);
      if (result == 1)
      {
        printf("Recv buf addr: %#lx size: %d\n",
               (unsigned long)comp.buffer, comp.size);
        recv_pkt_count++;
      }
    }
  }
  printf("Receiver finished %d cmds\n", recv_pkt_count);
}

int main(int argc, char**argv)
{
  gxio_trio_context_t trio_context_body;
  gxio_trio_context_t* trio_context = &trio_context_body;
  gxpci_context_t gxpci_context_body;
  gxpci_context_t* gxpci_context = &gxpci_context_body;

  //
  // We must bind to a single CPU.
  //
  tmc_cpus_set_my_cpu(0);

  //
  // This indicates that we need to allocate an ASID ourselves,
  // instead of using one that is allocated somewhere else.
  //
  int asid = GXIO_ASID_NULL;

  parse_args(argc, argv);

  //
  // Get a gxio context.
  //
  int result = gxio_trio_init(trio_context, trio_index);
  VERIFY_ZERO(result, "gxio_trio_init()");

  result = gxpci_init(trio_context, gxpci_context, trio_index, g_mac);
  VERIFY_ZERO(result, "gxpci_init()");

  //
  // Set up a C2C send queue, transferring data from SIM0_MAC
  // to SIM1_MAC. The queue index must be the same for both
  // GXPCI_C2C_SEND type and GXPCI_C2C_RECV type.
  //

  unsigned int queue_index = QUEUE_INDEX;

  if (g_mac == SIM0_MAC) 
  { 
    result = gxpci_open_queue(gxpci_context, asid,
                              GXPCI_C2C_SEND,
                              SIM1_MAC_INDEX,
                              queue_index,
                              0,
                              0);
  }
  else
  {
    result = gxpci_open_queue(gxpci_context, asid,
                              GXPCI_C2C_RECV,
                              SIM0_MAC_INDEX,
                              queue_index,
                              RECV_PKT_HEADROOM,
                              RECV_BUFFER_SIZE);
  }
  VERIFY_ZERO(result, "gxpci_open_queue()");

  //
  // Allocate and register data buffers.
  //
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* buf_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
  assert(buf_mem);

  int num_huge_page = (MAP_LENGTH + tmc_alloc_get_huge_pagesize() - 1) / 
    tmc_alloc_get_huge_pagesize();

  //
  // Every page should be registered to IOTLB for TRIO DMA use.
  //
  for (int i = 0; i < num_huge_page; i++)
  {
    buf_mem = (void*)((intptr_t)buf_mem + i * tmc_alloc_get_huge_pagesize());
    result = gxpci_iomem_register(gxpci_context, buf_mem, 
                                  tmc_alloc_get_huge_pagesize());
    VERIFY_ZERO(result, "gxpci_iomem_register()");
  }

  // Run the test.
  if (g_mac == SIM0_MAC)
  {  
    printf("Sender initialization complete\n");

    do_send(gxpci_context, buf_mem);
  }
  else
  {
    printf("Receiver initialization complete\n");

    do_recv(gxpci_context, buf_mem);
  }
  
  return 0;
}
