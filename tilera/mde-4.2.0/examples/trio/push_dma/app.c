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
#include <errno.h>

#include <arch/sim.h>
#include <gxio/trio.h>
#include <tmc/alloc.h>
#include <tmc/task.h>


#define MAP_LENGTH (16 * 1024 * 1024)
#define MAX_BUFFER_SIZE (4 * 1024)

// We run two copies of this program, one on each peered simulator.
// We use a different TRIO MAC and bus address for each simulator.
#define SIM0_MAC 0
#define SIM1_MAC 1
#define SIM0_MAP_BUS_ADDR 0xdeadbeef0000ull
#define SIM1_MAP_BUS_ADDR 0xcafebad00000ull

// The MAC and bus addresses for this simulator.
int g_mac;
unsigned long long g_remote_bus_addr;
unsigned long long g_local_bus_addr;

#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)

#define VERIFY_ZERO(VAL, WHAT)                                  \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val != 0)                                             \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)


// Parses command line arguments in order to fill in the MAC and bus
// address variables.
void parse_args(int argc, char** argv)
{
  if (argc < 2)
    tmc_task_die("Usage: push_dma sim_number");
  int sim = atoi(argv[1]);
  
  if (sim == 0)
  {
    g_mac = SIM0_MAC;
    g_local_bus_addr = SIM0_MAP_BUS_ADDR;
    g_remote_bus_addr = SIM1_MAP_BUS_ADDR;
  }
  else
  {
    g_mac = SIM1_MAC;
    g_local_bus_addr = SIM1_MAP_BUS_ADDR;
    g_remote_bus_addr = SIM0_MAP_BUS_ADDR;
  }
  printf("mac=%d, local_bus_addr=%#llx, remote_bus_addr=%#llx\n",
         g_mac, g_local_bus_addr, g_remote_bus_addr);
}

// Create a MapMem region that maps incoming reads and writes to the
// specified backing memory.  Also, create a PIO region that allows us
// to send read and write requests to the remote simulator MapMem
// region via MMIO loads and stores.
void* setup_bus_mappings(gxio_trio_context_t* context, int asid,
                         void* backing_mem)
{
  // Allocate and initialize a memory map region.
  int map_index = gxio_trio_alloc_memory_maps(context, 1, 0, 0);
  VERIFY(map_index, "gxio_trio_alloc_memory_maps()");

  int err = gxio_trio_init_memory_map(context, map_index, backing_mem,
                                      MAP_LENGTH, asid, g_mac, g_local_bus_addr,
                                      GXIO_TRIO_ORDER_MODE_UNORDERED);
  VERIFY_ZERO(err, "gxio_trio_init_memory_map()");
  
  // Allocate and initialize a PIO region.
  int pio_region = gxio_trio_alloc_pio_regions(context, 1, 0, 0);
  VERIFY(pio_region, "gxio_trio_alloc_pio_regions()");

  err = gxio_trio_init_pio_region(context, pio_region, g_mac,
                                  g_remote_bus_addr, 0);
  VERIFY_ZERO(err, "gxio_trio_init_pio_region()");

  void* mmio = gxio_trio_map_pio_region(context, pio_region, MAP_LENGTH, 0);
  if (mmio == MAP_FAILED)
    tmc_task_die("Failure in gxio_trio_map_pio_region: MAP_FAILED: %s.",
                 gxio_strerror(-errno));
  printf("PIO MMIO address = %p\n", mmio);

  return mmio;
}

// Initialize a DMA queue to drive the push DMA ring.  We use bytes at
// the end of our registered page to store the command ring; earlier parts
// of the registered page are used later for source and destination
// buffers.
void setup_push_dma(gxio_trio_context_t* context, gxio_trio_dma_queue_t* queue,
                    int asid, void* backing_mem)
{
   // Allocate a push DMA ring.
  int push_ring = gxio_trio_alloc_push_dma_ring(context, 1, 0, 0);
  VERIFY(push_ring, "gxio_trio_alloc_push_dma_ring");

  // Bind the DMA ring to our MAC, and use registered memory to store
  // the command ring.
  size_t dma_ring_size = 512 * sizeof(gxio_trio_dma_desc_t);
  void* dma_ring_mem = backing_mem + MAP_LENGTH - dma_ring_size;
  int err = gxio_trio_init_push_dma_queue(queue, context, push_ring, g_mac,
                                          asid, 0, dma_ring_mem,
                                          dma_ring_size, 0);
  VERIFY_ZERO(err, "gxio_trio_init_push_dma_queue");
}

// Synchronization barrier between the two peered simulators.
// Initially, any reads to a remote simulator return -1 because no
// memory is mapped.  Later, once memory is mapped, reads will return
// zero (mmap returns zeroed memory), and then this code starts
// advancing a generation number that must be synchronized whenever
// the app calls this method.
void barrier(void* mmio, void* backing_mem)
{
  static uint64_t generation = 0;

  // Advance our generation, then wait for remote app to do the same.
  generation++;
  *((uint64_t*)backing_mem) = generation;
  while (gxio_trio_read_uint64(mmio) != generation)
  {}
}


// Issue a DMA command for every possible size between 1 and 4096 bytes.
void issue_push_dmas(gxio_trio_dma_queue_t* queue, char* source,
                     unsigned long long dest)
{
  int64_t slot;
  for (int i = 0; i < MAX_BUFFER_SIZE; i++)
  {
    size_t size = i + 1;
    gxio_trio_dma_desc_t desc = {{
        .va = (intptr_t)source,
        .bsz = 0,
        .c = 0,
        .notif = 0,
        .smod = 0,
        .xsize = size,
        .io_address = dest,
      }};
    slot = gxio_trio_dma_queue_reserve(queue, 1);
    assert(slot >= 0);
    gxio_trio_dma_queue_put_at(queue, desc, slot);

    dest += (size + 64);
  }

  // Wait for all the DMAs to complete.
  while (!gxio_trio_dma_queue_is_complete(queue, slot, 1))
  {}
  printf("DMAs complete\n");
}

// Verify the data writes from issue_push_dmas().
void check_dma_results(char* source, char* dest)
{
  for (int i = 0; i < MAX_BUFFER_SIZE; i++)
  {
    size_t size = i + 1;
    assert(memcmp(dest, source, size) == 0);
    
    dest += (size + 64);
  }
  printf("Verified results\n");
}


int main(int argc, char**argv)
{
  gxio_trio_context_t context_body;
  gxio_trio_context_t* context = &context_body;

  parse_args(argc, argv);

  // Get a context.
  int result = gxio_trio_init(context, 0);
  VERIFY_ZERO(result, "gxio_trio_init()");

  // Allocate an ASID and bind some memory.  This huge page will be
  // mapped to the bus at g_local_bus_addr, and will also be used as
  // source data for push DMA.  Both MapMem regions and DMA commands
  // require that their memory be registered via gxio_trio_register_page(),
  // so it's convenient to use the same memory for both.
  int asid = gxio_trio_alloc_asids(context, 1, 0, 0);
  VERIFY(asid, "gxio_trio_alloc_asids()");
  
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* backing_mem = tmc_alloc_map(&alloc, MAP_LENGTH);
  assert(backing_mem);

  gxio_trio_register_page(context, asid, backing_mem, MAP_LENGTH, 0);

  // Create MapMem and PIO regions.
  void* mmio = setup_bus_mappings(context, asid, backing_mem);

  // Create push DMA queue.
  gxio_trio_dma_queue_t queue_body;
  gxio_trio_dma_queue_t* const queue = &queue_body;
  setup_push_dma(context, queue, asid, backing_mem);

  barrier(mmio, backing_mem);
  printf("Initialization complete\n");

  // Fill a buffer with DMA source data.
  char* source = (char*)backing_mem + 64;
  for (int i = 0; i < MAX_BUFFER_SIZE; i++)
    source[i] = i + (i >> 8) + 1;
  __insn_mf();
  
  // Run the test.
  issue_push_dmas(queue, source,
                  g_remote_bus_addr + 64 + MAX_BUFFER_SIZE);
  barrier(mmio, backing_mem);
  check_dma_results(source, (char*)backing_mem + 64 + MAX_BUFFER_SIZE);
  barrier(mmio, backing_mem);
  
  return 0;
}
