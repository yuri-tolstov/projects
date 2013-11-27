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

#include <gxio/mica.h>

#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/task.h>

#include <arch/mica_comp_ctx_user_def.h>

#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)

/*
 * Allocate a page of memory and register that page with MiCA
 * so that it can read and write the page's data.
 */
void* alloc_mica_page(gxio_mica_context_t* cb, size_t page_size)
{
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  if (page_size > getpagesize())
    tmc_alloc_set_huge(&alloc);
  void* page0 = tmc_alloc_map(&alloc, page_size);
  if (page0) {
    // Register the page with MiCA so that it can perform DMA operations
    // to and from this memory.
    int result = gxio_mica_register_page(cb, page0, page_size, 0);
    VERIFY(result, "gxio_mica_register_page()");
  }
  return page0;
}

int
main(int argc, char** argv)
{
  int result;

  // Bind to a single cpu.
  cpu_set_t cpus;
  result = tmc_cpus_get_my_affinity(&cpus);
  VERIFY(result, "tmc_cpus_get_my_affinity()");
  result = tmc_cpus_set_my_cpu(tmc_cpus_find_first_cpu(&cpus));
  VERIFY(result, "tmc_cpus_set_my_cpu()");

  // Initialize a MiCA crypto context.
  gxio_mica_context_t cb; 
  result = gxio_mica_init(&cb, GXIO_MICA_ACCEL_COMP, 0);
  VERIFY(result, "gxio_mica_init()");

  // Allocate a huge page to hold our data going to and from MiCA.
  size_t page_size = (1 << 24);
  void* page0 = alloc_mica_page(&cb, page_size);
  if (page0 == NULL) {
    // Failed to allocate a Huge page, so use a standard size page
    page_size = 64 * 1024;
    page0 = alloc_mica_page(&cb, page_size);
  }
  assert(page0);

  // Allocate a second page for the output data. This is not required
  // but keeps the code simple.
  void* page1 = alloc_mica_page(&cb, page_size);
  assert(page1);
  void* page2 = alloc_mica_page(&cb, page_size);
  assert(page2);

  // Make some source data in the registered memory.  Get an
  // address, pick a length, and put a pattern in memory that is 
  // probably somewhat compressible.
  void* src_data = page0;

  uint64_t sample_data[] = {
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0706050403020100,
    0x0f0e0d0c0b0a0908,
    0x1716151413121110,
    0x1f1e1d1c1b1a1918,
  };
  int sample_data_len = sizeof(sample_data);

  // Create a destination area in the registered memory, that is 
  // the same length as the source data.  This memory will already
  // have been zeroed by tmc_alloc_map().  Since we are deflating
  // the data first it should fit in the destination memory.
  void* dst_data = page1;
  void* dst2_data = page2;

  memset(dst_data, 'b', sizeof(sample_data));

  gxio_mica_opcode_t opcode = {{ 0 }};
  opcode.engine_type = MICA_COMP_CTX_USER_OPCODE__ENGINE_TYPE_VAL_COMP;
  opcode.size = sizeof(sample_data);
  opcode.dst_size = 3; // Allow compressed data to be 2x the size of the source.
  memcpy(src_data, sample_data, sizeof(sample_data));

  // Start the compression
  gxio_mica_start_op(&cb, src_data, dst_data, NULL, opcode);

  // Wait until the operation is done.
  while (gxio_mica_is_busy(&cb));

  // Look at status registers.  Make sure that that data actually 
  // compressed.
  MICA_COMP_CTX_USER_CONTEXT_STATUS_t ctx_status;
  ctx_status.word = __gxio_mmio_read(cb.mmio_context_user_base +
                                     MICA_COMP_CTX_USER_CONTEXT_STATUS);
  if (ctx_status.dst_ovf)
  {
    printf("Error: destination overflow\n");
    return 1;
  }

  MICA_COMP_CTX_USER_DEF_CONTEXT_STATUS_t status;
  status.word = __gxio_mmio_read(cb.mmio_context_user_base +
                                 MICA_COMP_CTX_USER_DEF_CONTEXT_STATUS);

  printf("Compressed %zd bytes to %d bytes.\n", sizeof(sample_data),
         status.sts_output_bytes);

  // Compare the source data to the destination data, make sure it
  // has changed.
  if (memcmp(src_data, dst_data, sample_data_len) == 0)
    printf("Source data and destination data match (and should not)!\n");

  // Now decompress the compressed data.  It should match
  // the original source data.

  opcode.size = status.sts_output_bytes;
  // Allow decompressed to be 8x larger. See mica.h for details.
  opcode.dst_size = 6;
  opcode.engine_type = MICA_COMP_CTX_USER_OPCODE__ENGINE_TYPE_VAL_DECOMP;

  // Start the decompression
  gxio_mica_start_op(&cb, dst_data, dst2_data, NULL, opcode);

  // Wait until the operation is done.
  while (gxio_mica_is_busy(&cb))
    ;

  if (memcmp(src_data, dst_data, sample_data_len) == 0)
    printf("Source data and destination data match (and should not)!\n");

  if (memcmp(src_data, dst2_data, sample_data_len))
    printf("Source data and destination data don't match!\n");
  else
    printf("Decompressed data matched original data.\n");

  return 0;
}
