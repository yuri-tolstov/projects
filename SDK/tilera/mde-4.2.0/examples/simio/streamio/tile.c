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

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <arch/sim.h>
#include <gxio/trio.h>
#include <tmc/alloc.h>
#include <tmc/task.h>

#define VERIFY(VAL, WHAT)                                        \
  do {                                                           \
    int __val = (VAL);                                           \
    if (__val < 0)                                               \
      tmc_task_die("Failure in '%s': %d: %s.",                   \
                   (WHAT), __val, gxio_strerror(__val));         \
  } while (0)

#define VERIFY_ZERO(VAL, WHAT)                                   \
  do {                                                           \
    int __val = (VAL);                                           \
    if (__val != 0)                                              \
      tmc_task_die("Failure in '%s': %d: %s.",                   \
                   (WHAT), __val, gxio_strerror(__val));         \
  } while (0)


int main()
{
  gxio_trio_context_t context_body;
  gxio_trio_context_t* context = &context_body;
  
  int mac = 0;

  // Get a context.
  int result = gxio_trio_init(context, 0);
  VERIFY_ZERO(result, "gxio_trio_init()");

  // Allocate an ASID and bind some memory.
  int asid = gxio_trio_alloc_asids(context, 1, 0, 0);
  VERIFY(asid, "gxio_trio_alloc_asids()");
  
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  unsigned char* backing_mem = tmc_alloc_map(&alloc, TILE_WINDOW_SIZE);
  assert(backing_mem);

  gxio_trio_register_page(context, asid, backing_mem, TILE_WINDOW_SIZE, 0);

  // Allocate and initialize a memory map region.
  int map_index = gxio_trio_alloc_memory_maps(context, 1, 0, 0);
  VERIFY(map_index, "gxio_trio_alloc_memory_maps()");

  result = gxio_trio_init_memory_map(context, map_index, backing_mem,
                                     TILE_WINDOW_SIZE, asid, mac,
                                     TILE_WINDOW_START,
                                     GXIO_TRIO_ORDER_MODE_UNORDERED);
  VERIFY_ZERO(result, "gxio_trio_init_memory_map()");
  
  // Allocate and initialize a PIO region.
  int pio_region = gxio_trio_alloc_pio_regions(context, 1, 0, 0);
  VERIFY(pio_region, "gxio_trio_alloc_pio_regions()");

  result = gxio_trio_init_pio_region(context, pio_region, mac,
                                     REMOTE_WINDOW_START, 0);
  VERIFY_ZERO(result, "gxio_trio_init_pio_region()");

  void* mmio = gxio_trio_map_pio_region(context, pio_region,
                                        REMOTE_WINDOW_SIZE, 0);
  printf("MMIO address = %p\n", mmio);

  // Store a value and make sure we can read it back.
  gxio_trio_write_uint64(mmio, 17);
  printf("Read back %lld\n", (unsigned long long)gxio_trio_read_uint64(mmio));

  // Wait for the remote side to write a flag indicating it's done.
  while (((volatile unsigned char*)backing_mem)[FLAG_OFFSET] == 0)
  {}
}
