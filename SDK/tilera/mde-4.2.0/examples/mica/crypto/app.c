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

#include <tmc/alloc.h>

#include <gxcr/gxcr.h>
#include <gxio/mica.h>

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
#define TEST_MEM_SIZE 1024

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
  gxio_mica_init(&cb, GXIO_MICA_ACCEL_CRYPTO, 0);

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

  // Create a destination area for encrypted in the registered memory, that is 
  // the same length as the source data.  This memory will already
  // have been zeroed by tmc_alloc_map().
  void* dst_data = src_data + TEST_MEM_SIZE;

  // Create another destination area in the registered memory for decrypting
  // the encrypted data.
  void* dst2_data = dst_data + TEST_MEM_SIZE;

  unsigned char key[GXCR_AES_128_KEY_SIZE];
  memcpy(key, "ABCDEFGHIJKLMNOP", sizeof(key));

  // Create an area in the registered memory for metadata needed by the
  // shim.  The gxcr_context structures go in this memory.
  void* enc_metadata = dst2_data + TEST_MEM_SIZE;
  void* dec_metadata = enc_metadata + TEST_MEM_SIZE;

  // Make a context for encrypting data and running a digest on it.
  gxcr_context_t encrypt_context;
  result = gxcr_init_context(&encrypt_context, enc_metadata, TEST_MEM_SIZE,
                             GXCR_CIPHER_AES_CBC_128, GXCR_DIGEST_MD5,
                             key, NULL, 1, 1, 0);
  VERIFY(result, "gxcr_init_context()");

  // Make a context for running a digest on encrypted data, then decrypting
  // the data.
  gxcr_context_t decrypt_context;
  result = gxcr_init_context(&decrypt_context, dec_metadata, TEST_MEM_SIZE,
                             GXCR_CIPHER_AES_CBC_128, GXCR_DIGEST_MD5,
                             key, NULL, 0, 0, 0);
  VERIFY(result, "gxcr_init_context()");

  // Encrypt the source data, using the hardware context and the
  // encryption context.
  result = gxcr_do_op(&cb, &encrypt_context, src_data, dst_data, TEST_LEN);
  VERIFY(result, "gxcr_do_op()");

  // Look at the digest, which is appended to the output data.
  int enc_digest_len = GXCR_MD5_DIGEST_SIZE;
  void* enc_digest = dst_data + TEST_LEN;

  // Verify that the source data actually got encrypted.
  if (!memcmp(src_data, dst_data, TEST_LEN))
    printf("Source data and encrypted data are identical!\n");

  printf("Digest: ");
  for (int i = 0; i < enc_digest_len; i++)
    printf("%02x", *((unsigned char*)enc_digest + i));
  printf("\n");

  // Decrypt the encrypted data, and compare to the original source.
  // They should match.
  result = gxcr_do_op(&cb, &decrypt_context, dst_data, dst2_data,
                      TEST_LEN);
  VERIFY(result, "gxcr_do_op()");

  if (memcmp(src_data, dst2_data, TEST_LEN))
  {
    printf("Source data and decrypted data don't match!\n");
  }
  // Compare the digests.  They should match too.
  void* dec_digest = dst2_data + TEST_LEN;

  if (memcmp(enc_digest, dec_digest, enc_digest_len))
  {
    printf("Digests don't match!\n");
    for (int i = 0; i < enc_digest_len; i++)
      printf("%02x", *((unsigned char*)dec_digest + i));
    printf("\n");
  }
  return result;
}
