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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <tmc/alloc.h>

#include <arch/sim.h>

#include <gxio/mpipe.h>

#include <tmc/cpus.h>
#include <tmc/task.h>


// Define this to verify a bunch of facts about each packet.
#define PARANOIA


// Align "p" mod "align", assuming "p" is a "void*".
#define ALIGN(p, align) do { (p) += -(long)(p) & ((align) - 1); } while(0)


#define VERIFY(VAL, WHAT)                                       \
  do {                                                          \
    long long __val = (VAL);                                    \
    if (__val < 0)                                              \
      tmc_task_die("Failure in '%s': %lld: %s.",                \
                   (WHAT), __val, gxio_strerror(__val));        \
  } while (0)



int
main(int argc, char** argv)
{
  char* link_name = "gbe0";

  // There are 1000 packets in "input.pcap".
  int num_packets = 1000;

  // Parse args.
  for (int i = 1; i < argc; i++)
  {
    char* arg = argv[i];

    if (!strcmp(arg, "--link") && i + 1 < argc)
    {
      link_name = argv[++i];
    }
    else if (!strcmp(arg, "-n") && i + 1 < argc)
    {
      num_packets = atoi(argv[++i]);
    }
    else
    {
      tmc_task_die("Unknown option '%s'.", arg);
    }
  }


  int instance;

  gxio_mpipe_context_t context_body;
  gxio_mpipe_context_t* context = &context_body;

  gxio_mpipe_iqueue_t iqueue_body;
  gxio_mpipe_iqueue_t* iqueue = &iqueue_body;

  int result;

  
  // Bind to a single cpu.
  cpu_set_t cpus;
  result = tmc_cpus_get_my_affinity(&cpus);
  VERIFY(result, "tmc_cpus_get_my_affinity()");
  result = tmc_cpus_set_my_cpu(tmc_cpus_find_first_cpu(&cpus));
  VERIFY(result, "tmc_cpus_set_my_cpu()");


  // Get the instance.
  instance = gxio_mpipe_link_instance(link_name);
  if (instance < 0)
    tmc_task_die("Link '%s' does not exist.", link_name);

  // Start the driver.
  result = gxio_mpipe_init(context, instance);
  VERIFY(result, "gxio_mpipe_init()");

  gxio_mpipe_link_t link;
  result = gxio_mpipe_link_open(&link, context, link_name, 0);
  VERIFY(result, "gxio_mpipe_link_open()");


  // Allocate one huge page to hold our buffer stack, notif ring, and
  // packets.  This should be more than enough space.
  size_t page_size = tmc_alloc_get_huge_pagesize();
  tmc_alloc_t alloc = TMC_ALLOC_INIT;
  tmc_alloc_set_huge(&alloc);
  void* page = tmc_alloc_map(&alloc, page_size);
  assert(page);

  void* mem = page;


  // Allocate a NotifRing.
  result = gxio_mpipe_alloc_notif_rings(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_rings()");
  int ring = result;

  // Init the NotifRing.
  size_t notif_ring_entries = 128;
  size_t notif_ring_size = notif_ring_entries * sizeof(gxio_mpipe_idesc_t);
  result = gxio_mpipe_iqueue_init(iqueue, context, ring,
                                  mem, notif_ring_size, 0);
  VERIFY(result, "gxio_mpipe_iqueue_init()");
  mem += notif_ring_size;


  // Allocate a NotifGroup.
  result = gxio_mpipe_alloc_notif_groups(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_notif_groups()");
  int group = result;

  // Allocate a bucket.
  int num_buckets = 1;
  result = gxio_mpipe_alloc_buckets(context, num_buckets, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buckets()");
  int bucket = result;

  // Init group and bucket.
  gxio_mpipe_bucket_mode_t mode = GXIO_MPIPE_BUCKET_ROUND_ROBIN;
  result = gxio_mpipe_init_notif_group_and_buckets(context, group,
                                                   ring, 1,
                                                   bucket, num_buckets, mode);
  VERIFY(result, "gxio_mpipe_init_notif_group_and_buckets()");


  // Allocate a buffer stack.
  result = gxio_mpipe_alloc_buffer_stacks(context, 1, 0, 0);
  VERIFY(result, "gxio_mpipe_alloc_buffer_stacks()");
  int stack_idx = result;

  // Total number of buffers.
  unsigned int num_buffers = notif_ring_entries;

  // Initialize the buffer stack.  Must be aligned mod 64K.
  ALIGN(mem, 0x10000);
  size_t stack_bytes = gxio_mpipe_calc_buffer_stack_bytes(num_buffers);
  gxio_mpipe_buffer_size_enum_t buf_size = GXIO_MPIPE_BUFFER_SIZE_1664;
  result = gxio_mpipe_init_buffer_stack(context, stack_idx, buf_size,
                                        mem, stack_bytes, 0);
  VERIFY(result, "gxio_mpipe_init_buffer_stack()");
  mem += stack_bytes;

  ALIGN(mem, 0x10000);

  // Register the entire huge page of memory which contains all the buffers.
  result = gxio_mpipe_register_page(context, stack_idx, page, page_size, 0);
  VERIFY(result, "gxio_mpipe_register_page()");

  // Push some buffers onto the stack.
  for (int i = 0; i < num_buffers; i++)
  {
    gxio_mpipe_push_buffer(context, stack_idx, mem);
    mem += 1664;
  }

  // Paranoia.
  assert(mem <= page + page_size);


  // Register for packets.
  gxio_mpipe_rules_t rules;
  gxio_mpipe_rules_init(&rules, context);
  gxio_mpipe_rules_begin(&rules, bucket, num_buckets, NULL);
  result = gxio_mpipe_rules_commit(&rules);
  VERIFY(result, "gxio_mpipe_rules_commit()");


  // Allow packets to flow.
  sim_enable_mpipe_links(instance, -1);


  // Process packets.
  int handled = 0;
  while (handled < num_packets)
  {
    // Wait for next packet.
    gxio_mpipe_idesc_t idesc;
    result = gxio_mpipe_iqueue_get(iqueue, &idesc);
    VERIFY(result, "gxio_mpipe_iqueue_get()");

    // NOTE: On the simulator, we run with ":stall", and so we will
    // never drop packets due to running out of buffers.  Otherwise,
    // we would need to specially handle packets with "idesc.be",
    // for example, by using "gxio_mpipe_iqueue_drop_if_bad()".

    // Analyze the packet descriptor.
    int size = idesc.l2_size;
    printf("Packet %d has size %d\n", handled, size);

#ifdef PARANOIA

    // MAC Error.
    // The default classifier drops such packets (via packet_has_error()).
    assert(!idesc.me);

    // Truncation Error.
    // The default classifier drops such packets (via packet_has_error()).
    assert(!idesc.tr);

    // CRC Error.
    // The default classifier drops such packets (via packet_has_error()).
    assert(!idesc.ce);

    // Cut through.
    // The default classifier drops such packets.
    assert(!idesc.ct);

    // Checksum.
    // The default classifier requests checksums for IPv4/IPv6 TCP/UDP
    // packets (except UDP packets whose udp checksum is zero).
    assert(idesc.cs && idesc.csum_seed_val == 0xffff);

    // Notification Ring Select (vs let the load balancer pick a ring).
    // The default classifier always uses 0.
    assert(!idesc.nr);

    // Destination (0 = drop, 1 = deliver, 2 = deliver only descriptor).
    // The default classifier only uses 0 and 1, and 0 causes drops.
    assert(idesc.dest == 1);

    // General Purpose Sequence Number Enable.
    // The default classifier currently uses 0.
    //--assert(idesc.sq);

    // Enable TimeStamp insertion.
    // The default classifier always uses 1.
    assert(idesc.ts);

    // Packet Sequence Number Enable.
    // The default classifier always uses 1.
    assert(idesc.ps);

    // Buffer Error.
    // ISSUE: This can happen if the buffer stack is empty.
    assert(!idesc.be);

    // The "vlan" (or 0xFFFF).
    uint16_t vlan = gxio_mpipe_idesc_get_vlan(&idesc);
    assert(vlan == 0xFFFF);

    // The ethertype (or zero).  IPv4 = 0x0800, IPv6 = 0x86DD.
    uint16_t ethertype = gxio_mpipe_idesc_get_ethertype(&idesc);
    assert(ethertype == 0x0800);

    // The byte index of the start of the l2 data.
    uint8_t l2_offset = gxio_mpipe_idesc_get_l2_offset(&idesc);
    assert(l2_offset == 0);

    // The byte index of the start of the l3 header.
    uint8_t l3_offset = gxio_mpipe_idesc_get_l3_offset(&idesc);
    assert(l3_offset >= l2_offset + 14);

    // The byte index of the start of the l4 header (or zero).
    uint8_t l4_offset = gxio_mpipe_idesc_get_l4_offset(&idesc);
    assert(l4_offset > l3_offset);

    // Check for bad checksums and buffer errors.
    // NOTE: This checks "gxio_mpipe_idesc_get_status(&idesc)".
    assert(!gxio_mpipe_idesc_is_bad(&idesc));

    if (sim_is_simulator())
    {
      // This is rarely true on hardware.
      //--assert(idesc.packet_sqn == handled);
    }
    else
    {
      // This is only true if no other app has used the bucket yet.
      // FIXME: The "gp_sqn" is not implemented on the simulator. 
      //--assert(idesc.gp_sqn == handled);
    }

#endif

    // Just "drop" the packet.
    gxio_mpipe_iqueue_drop(iqueue, &idesc);

    handled++;
  }

  return 0;
}
