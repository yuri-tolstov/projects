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
#include <pthread.h>

#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/sync.h>
#include <tmc/task.h>
#include <tmc/interrupt.h>
#include <tmc/ipi.h>

#include <gxio/mica.h>

#include "queue.h"

#include <gxcr/tokens/esp_ipv4_tunnel_ibound.h>
#include <gxcr/tokens/esp_ipv4_tunnel_obound.h>

// The number of packets to process in this application.
#define NUM_PACKETS 10000

// The number of elements in the MiCA queue.
#define QUEUE_SIZE 32

// The number of worker CPUs in this application.
#define NUM_WORKERS 4

// The set of CPUs in our application.
static cpu_set_t cpus;

// Help synchronize thread creation.
static tmc_sync_barrier_t barrier;

// Helper macro to check error codes from function calls.
#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)

// Wrapper to call packet processing function with different signature.
static inline int
ipsec_obound_process_packet_start_func(gxio_mica_context_t* mica_context,
                                       void* op_sa,
                                       void* packet, int packet_len,
                                       void* dst, int dst_len,
                                       int seq_num)
{
  return ipsec_esp_ipv4_tunnel_obound_process_packet_start(mica_context,
                                                           op_sa,
                                                           packet, packet_len,
                                                           dst, dst_len);
}



// This little demo program just allocates memory in chunks out
// of the page that has been registered with the MiCA shim.
// It is an overly-simplistic scheme, but useful for a simple example.
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

typedef struct {
  ipsec_queue_t queue;
  int interrupt_count;
} ipsec_queue_and_metadata_t;

ipsec_queue_and_metadata_t queue_and_metadata[NUM_WORKERS];

#if INTERRUPT_MODEL

// This is the function that is called when the tile receives an IPI event.
// The IPI event is associated with the queue to be serviced.
void
completion_handler(void* arg)
{
  ipsec_queue_and_metadata_t* qp = arg;

  qp->interrupt_count++;
  queue_service(&qp->queue);
}
#endif

static void*
main_aux(void* arg)
{
ipsec_queue_desc_t desc[QUEUE_SIZE];
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
  const int hmac_key_len = sizeof(hmac_key);

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

  gxcr_ipsec_precalc(&mica_context,
                     &obound_sa.ipsec_sa,
                     hmac_precalc_mem, 
                     gxcr_ipsec_precalc_calc_memory_size(&obound_params,
                                                         hmac_key_len),
                     hmac_key, hmac_key_len);

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

  int max_obound_packet_len = packet_len + obound_sa.additional_packet_len;

  int rank = (long) arg;

  ipsec_queue_t* lqp = &queue_and_metadata[rank].queue;

  result = queue_init(lqp, &mica_context, QUEUE_SIZE);
  VERIFY(result, "queue_init()");

  unsigned char* obound_packet[QUEUE_SIZE];

  for (int i = 0; i < QUEUE_SIZE; i++)
  {
    obound_packet[i] = mem_alloc_ptr;
    mem_alloc_ptr += ALLOC_MEM_SIZE;

    desc[i].sa = &obound_sa.ipsec_sa;
    desc[i].src = packet;
    desc[i].dst = obound_packet[i];
    desc[i].src_packet_len = packet_len;
    desc[i].dst_packet_len = max_obound_packet_len;
    desc[i].packet_start_func = ipsec_obound_process_packet_start_func;
  }


  int cpu = tmc_cpus_find_nth_cpu(&cpus, rank);
  printf("main_aux: cpu = %d rank = %d\n", cpu, rank);

  result = tmc_cpus_set_my_cpu(cpu);
  VERIFY(result, "tmc_cpus_set_my_cpu()");

#if INTERRUPT_MODEL
  // Activate the IPI capability.  
  int res = tmc_ipi_activate();
  printf("tmc_ipi_activate returned %d\n", res);

  int ipi_level = 0;
  int64_t ipi_event = tmc_ipi_event_alloc_first_available(ipi_level);

  // In an actual application, the function
  // tmc_ipi_event_allocate_first_available() could be used, and a 
  // translation in the completion handler could relate the event to
  // the queue.
  if (ipi_event < 0)
  {
    printf("Could not allocate desired event\n");
    return NULL;
  }

  tmc_ipi_event_install(ipi_level, ipi_event, completion_handler,
                        &queue_and_metadata[rank]);

  // Set up a completion interrupt.
  gxio_mica_cfg_completion_interrupt(&mica_context, cpu, ipi_event);
#endif

  // Start queue processing at the same time on all cores, just to keep
  // the results more or less in sync.
  tmc_sync_barrier_wait(&barrier);

  // Put packets on the queue.  The function queue_put() is a blocking
  // function which automatically services the queue if the hardware is
  // not busy.
  for (int i = 0; i < QUEUE_SIZE; i++)
    queue_put(lqp, &desc[i]);

  int completed_count = 0;
#if INTERRUPT_MODEL
  int slot = 0;
  while (completed_count < NUM_PACKETS)
  {
    ipsec_queue_desc_t* res_desc = NULL;
    int count = 0;

    // Count how many done packets we get off the queue, and replenish
    // the queue with more operations.
    while (queue_try_get(lqp, &res_desc) == 0)
    {
      count++;
    }
    if (count > 0)
    {
      for (int slot_count = 0; slot_count < count; slot_count++)
      {
        queue_put(lqp, &desc[slot]);
        slot++;
        if (slot >= QUEUE_SIZE)
          slot = 0;
      }
      completed_count += count;
    }

    if (completed_count && ((completed_count % 1000) == 0))
    printf("rank %d completed %d packets, interrupt count = %d\n",
           rank, completed_count, queue_and_metadata[rank].interrupt_count);
  }  

  tmc_ipi_deactivate();

#else
  while (completed_count < NUM_PACKETS)
  {
    // This function can be called many times in the loop, whenever it
    // is convenient.
    queue_service(lqp);

    ipsec_queue_desc_t* res_desc = NULL;
    int count = 0;
    // count how many done packets we get off the queue
    while (queue_try_get(lqp, &res_desc) == 0)
    {
      completed_count++;
      count++;
    }
    if (count > 0)
    {
      // reserve that many slots
      int slot = queue_try_reserve(lqp, count);
      if (slot < 0)
      {
        printf("Couldn't reserve slots!\n");
        return NULL;
      }
      for (int slot_count = 0; slot_count < count; slot_count++)
      {
        queue_put_at(lqp, &desc[slot], slot);
        slot++;
        if (slot >= QUEUE_SIZE) slot = 0;
      }
    }

    if (completed_count && ((completed_count % 1000) == 0))
    printf("rank %d completed %d packets\n", rank, completed_count);
  }  
#endif

  return 0;
}


int
main(int argc, char** argv)
{
  int result;
  int num_workers = NUM_WORKERS;

#if INTERRUPT_MODEL
  printf("Interrupt-driven Queuing Example\n");
#else
  printf("Polling Queuing Example\n");
#endif
  printf("Running %s with %d workers\n", argv[0], num_workers);

  // Determine the available cpus.
  result = tmc_cpus_get_my_affinity(&cpus);
  VERIFY(result, "tmc_cpus_get_my_affinity()");

  if (tmc_cpus_count(&cpus) < num_workers)
    tmc_task_die("Insufficient cpus.");

  tmc_sync_barrier_init(&barrier, num_workers);

#if INTERRUPT_MODEL
  // Activate the IPI capability.  
  result = tmc_ipi_init(NULL);
  printf("tmc_ipi_init returned %d\n", result);
#endif

  if (num_workers > 1)
  {
    pthread_t threads[num_workers];
    for (int i = 0; i < num_workers; i++)
    {
      if (pthread_create(&threads[i], NULL, main_aux, (void*)(intptr_t)i) != 0)
        tmc_task_die("Failure in 'pthread_create()'.");
    }
    for (int i = 0; i < num_workers; i++)
    {
      if (pthread_join(threads[i], NULL) != 0)
        tmc_task_die("Failure in 'pthread_join()'.");
    }
  }
  else
  {
    (void)main_aux((void*)(intptr_t)0);
  }

  return 0;
}
