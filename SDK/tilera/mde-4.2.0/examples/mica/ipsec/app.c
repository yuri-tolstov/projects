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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/task.h>

#include <gxio/mica.h>
#include <gxcr/gxcr.h>
#include <gxcr/ipsec.h>

#include <gxcr/tokens/esp_ipv4_tunnel_ibound.h>
#include <gxcr/tokens/esp_ipv4_tunnel_obound.h>

#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)

// This little demo program just allocates memory in 2k chunks out
// of the 64k huge page that has been registered with the MiCA shim.
// It is an overly simplistic scheme, but useful for a simple example.
#define ALLOC_MEM_SIZE 2048

// Some sample packet data.
static unsigned char sample_packet_data[] = {
  // ipv4 inner header
  // length, header length & version
  0x45,0x00,0x00,0x5a,
  // id, flags, frag
  0x00,0x00,0x00,0x00,
  // ttl, protocol, checksum
  0x99,0x04,0xa8,0x19,
  // src ip address
  0xd8,0x2c,0xf1,0x5d,
  // dst ip address
  0x0d,0x2a,0xa2,0xd2,

  // payload
  0x20,0x33,0xce,0x04,
  0x7f,0x5b,0xfa,0xb1,
  0x13,0xc7,0x8e,0x80,
  0x55,0x04,0x00,0x00,
  0xb4,0xc2,0x9d,0xd8,
  0x00,0x00,0x00,0x01,
  0x61,0xae,0x5a,0x6c,
  0x20,0xcc,0x87,0x59,
  0x51,0x9a,0x8d,0x9a,
  0x9a,0x0a,0x15,0x37,
  0x16,0xb1,0x53,0x91,
  0x0c,0xd1,0x1e,0x47,
  0xbf,0x04,0x06,0x6b,
  0xbe,0x0c,0x5b,0x1f,
  0x89,0xef,0x63,0x75,
  0xc5,0xa2,0x91,0x8b,
  0x60,0x3d,0x0f,0x39,
  0xdd,0xee,
};

void hexprint_packet(unsigned char* packet, int len)
{
  for (int i = 0; i < len; i++)
  {
    printf("%02x", *(packet + i));
  }
  printf("\n");
}

// This takes an ipv4 packet, runs it through outbound processing (thus
// encrypting and encapsulating it, and making a digest).  It takes that
// resulting packet and runs it through inbound processing.
//
void obound_ibound_sample()
{
  gxio_mica_context_t mica_context; 
  gxio_mica_init(&mica_context, GXIO_MICA_ACCEL_CRYPTO, 0);

  // Allocate a huge page to hold our metadata for the IPSec SAs.
  size_t page_size = (1 << 24);
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* page0 = tmc_alloc_map(&alloc, page_size);
  assert(page0);

  // Register the page with MiCA.
  int result = gxio_mica_register_page(&mica_context, page0, page_size, 0);
  VERIFY(result, "gxio_mica_register_page()");

  gxcr_ipsec_params_t obound_params = {
    .token_template = &esp_ipv4_tunnel_obound,
    .spi = 1002,
    .seqnum_size = 32,
    .seqnum_replay_window_size = 64,
    .cipher = GXCR_CIPHER_AES_CBC_128,
    .digest = GXCR_DIGEST_MD5,
    .explicit_iv = 1,
    .inbound = 0,
    .hmac_mode = 1,
  };

  // This points to the next portion of unallocated memory that has
  // been registered with the MiCA shim.  This overly-simplified method
  // is for demo purposes only.
  unsigned char* mem_alloc_ptr = page0;

  unsigned char* obound_sa_mem = mem_alloc_ptr;
  mem_alloc_ptr += ALLOC_MEM_SIZE;
  int obound_sa_memsize = gxcr_ipsec_calc_sa_bytes(&obound_params);
  
  ipsec_esp_ipv4_tunnel_obound_sa_t obound_sa;
  
  unsigned char key[GXCR_AES_128_KEY_SIZE];
  unsigned char iv[GXCR_AES_IV_SIZE];
  unsigned char hmac_key[GXCR_MD5_DIGEST_SIZE];
  int hmac_key_len = sizeof(hmac_key);

  memcpy(key, "0123456789abcdef", sizeof(key));
  memcpy(iv, "ghijklmnopqrstuv", sizeof(iv));
  memcpy(hmac_key, "0123456789abcdef", hmac_key_len);

  // This sets up the metadata memory (copies token template,
  // copies keys and iv to proper offsets of context record).
  // This is a general-purpose function that can be used with
  // any ipsec token.
  gxcr_ipsec_init_sa(&obound_sa.ipsec_sa, obound_sa_mem,
                     obound_sa_memsize, &obound_params,
                     key, iv);

  unsigned char* hmac_precalc_mem = mem_alloc_ptr;
  mem_alloc_ptr += ALLOC_MEM_SIZE;

  // This does additional initialization needed for HMAC digests.
  result =
    gxcr_ipsec_precalc(&mica_context,
                       &obound_sa.ipsec_sa,
                       hmac_precalc_mem, 
                       gxcr_ipsec_precalc_calc_memory_size(&obound_params,
                                                           hmac_key_len),
                       hmac_key, hmac_key_len);
  VERIFY(result, "gxcr_ipsec_precalc");

  int outer_ip_hdr_len = 20;
  int esp_hdr_len = 8;
  uint32_t outer_ip_hdr[] = {
    0x45000088,
    0x00000000,
    0x0000aaaa,
    0xaaaaaaaa,
    0xbbbbbbbb
  };

  // Get some memory for packets that is accessible from the MiCA shim.
  unsigned char* packet = mem_alloc_ptr;
  mem_alloc_ptr += ALLOC_MEM_SIZE;
  memcpy(packet, sample_packet_data, sizeof(sample_packet_data));

  // Now do operation-specific part.  These functions are specific to
  // the operation that the token performs.
  // This kind of function must be provided by the token writer.
  ipsec_esp_ipv4_tunnel_obound_setup(&obound_sa,
                                     outer_ip_hdr,
                                     outer_ip_hdr_len,
                                     esp_hdr_len);

  int packet_len = sizeof(sample_packet_data);
  unsigned char* obound_packet = mem_alloc_ptr;
  mem_alloc_ptr += ALLOC_MEM_SIZE;

  int max_obound_packet_len = packet_len + obound_sa.additional_packet_len;

  gxcr_ipsec_params_t ibound_params = {
    .token_template = &esp_ipv4_tunnel_ibound,
    .spi = 1002,
    .seqnum_size = 32,
    .seqnum_replay_window_size = 64,
    .cipher = GXCR_CIPHER_AES_CBC_128,
    .digest = GXCR_DIGEST_MD5,
    .explicit_iv = 1,
    .inbound = 1,
    .hmac_mode = 1,
  };

  // The inbound packet should be the same as the original packet
  // (but the ipv4 header (ttl and checksum) will have been modified)
  unsigned char* ibound_packet = mem_alloc_ptr;
  mem_alloc_ptr += ALLOC_MEM_SIZE;

  unsigned char* ibound_sa_mem = mem_alloc_ptr;
  mem_alloc_ptr += ALLOC_MEM_SIZE;
  int ibound_sa_memsize = gxcr_ipsec_calc_sa_bytes(&ibound_params);

  // This sets up the metadata memory (copies token template,
  // copies keys and iv to proper offsets of context record).
  // This is a general-purpose function that can be used with
  // any ipsec token.
  ipsec_esp_ipv4_tunnel_ibound_sa_t ibound_sa;
  gxcr_ipsec_init_sa(&ibound_sa.ipsec_sa, ibound_sa_mem,
                     ibound_sa_memsize, &ibound_params,
                     key, iv);

  // This does additional initialization needed for HMAC digests.
  result =
  gxcr_ipsec_precalc(&mica_context,
                     &ibound_sa.ipsec_sa,
                     hmac_precalc_mem, 
                     gxcr_ipsec_precalc_calc_memory_size(&ibound_params,
                                                         hmac_key_len),
                     hmac_key, hmac_key_len);
  VERIFY(result, "gxcr_ipsec_precalc");

  // Now do operation-specific part.  These functions are specific to
  // the operation that the token performs.
  // This kind of function must be provided by the token writer.
  ipsec_esp_ipv4_tunnel_ibound_setup(&ibound_sa,
                                     outer_ip_hdr_len,
                                     esp_hdr_len);

  int ibound_packet_len = packet_len + ibound_sa.additional_packet_len;

  // Setup complete.  Now do actual packet processing.  Run it
  // a few times for testing.
  for (int i = 0; i < 25; i++)
  {
    printf("\nIteration %d\n", i);
    ipsec_esp_ipv4_tunnel_obound_process_packet(&mica_context,
                                                &obound_sa,
                                                packet, packet_len,
                                                obound_packet,
                                                max_obound_packet_len);

    gxcr_result_token_t* res = gxcr_ipsec_result(&obound_sa.ipsec_sa);

    int actual_obound_packet_len = gxcr_result_packet_length(res);

    ipsec_esp_ipv4_tunnel_ibound_process_packet(&mica_context,
                                                &ibound_sa, 
                                                obound_packet,
                                                actual_obound_packet_len,
                                                ibound_packet,
                                                ibound_packet_len);

    printf("outbound errors = 0x%x\n", gxcr_result_error(res));

    printf("Original packet: ");
    hexprint_packet(packet, packet_len);

    printf("Outbound packet: ");
    hexprint_packet(obound_packet, actual_obound_packet_len);

    printf("Processed inbound packet: ");
    hexprint_packet(ibound_packet, ibound_packet_len);

    res = gxcr_ipsec_result(&ibound_sa.ipsec_sa);
    printf("inbound errors = 0x%x\n", gxcr_result_error(res));

    // Compare the original packet to the processed packet.  They
    // should be the same.
    if (memcmp(ibound_packet, packet, packet_len))
    {
      printf("Packets are different!\n");
      return;
    }
  }
}

int main(int argc, char* argv[])
{
  printf("Outbound/Inbound Sample\n");
  obound_ibound_sample();
}
