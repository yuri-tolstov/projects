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


#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)

#define TEST_LEN 128

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
  result = gxio_mica_init(&cb, GXIO_MICA_ACCEL_CRYPTO, 0);
  VERIFY(result, "gxio_mica_init");

  // Allocate a huge page to hold our data going to and from MiCA.
  size_t page_size = (1 << 24);
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* page0 = tmc_alloc_map(&alloc, page_size);
  assert(page0);

  // Register the page with MiCA so that it can perform DMA operations
  // to and from this memory.
  result = gxio_mica_register_page(&cb, page0, page_size, 0);
  VERIFY(result, "gxio_mica_register_page()");

  // Make some source data in the registered memory.  Get an
  // address, pick a length, and put a pattern in memory.
  void* src_data = page0;
  for (int i = 0; i < TEST_LEN; i++)
    ((char*)src_data)[i] = 'a' + (i % 26);

  // Create a destination area in the registered memory, that is 
  // the same length as the source data.  This memory will already
  // have been zeroed by tmc_alloc_map().
  void* dst_data = page0 + 1024;

  memset(dst_data, 'b', TEST_LEN);

  // Start the DMA transfer.
  gxio_mica_memcpy_start(&cb, dst_data, src_data, TEST_LEN);

  // Wait until the transfer is done.
  while (gxio_mica_is_busy(&cb));
  
  // Compare the source data to the destination data.
  char printstr[TEST_LEN];
  memcpy(printstr, dst_data, TEST_LEN);
  printstr[TEST_LEN - 1] = '\0';
  printf("*dst_data (x) = %s\n", printstr);

  if (strncmp(src_data, dst_data, TEST_LEN))
    printf("Source data and destination data don't match!\n");

  return result;
}
