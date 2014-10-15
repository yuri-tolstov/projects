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

#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/task.h>

#include <gxio/mica.h>
#include <gxcr/gxcr.h>
#include <gxcr/ipsec.h>


/**
 * This queuing API allows the programmer to batch operations to the MiCA shim.
 * It is to be used in conjunction with the IPsec API.  A queue is associated with
 * an initialized MiCA context.  Operations are submitted to the queue via the
 * queue_put() or queue_put_at() functions.  Operations are sent to and received
 * back from the hardware by the queue_service() routine.  Completed operations
 * are returned to the application via the queue_get() and queue_try_get()
 * functions.
 *
 * The queue can be interrupt driven, or polling can be used to advance queued
 * operations.  Generally speaking, polling is more efficient but interrupts can
 * make the programming model somewhat easier.  If interrupts are used, the
 * service routine must not be called directly from the application without
 * disabling the interrupt first.  See the example programs app_int.c and
 * app_poll.c for a demonstration of how to use each model.
 */

/** Function signature for packet processing function.
 * 
 * @param mica_context An initialized MiCA context.
 * @param op_sa An initialized operation-specific SA.
 * @param packet The packet to be processed.
 * @param packet_len Length, in bytes, of the packet to be processed.
 * @param dst Destination memory for the result packet.  If dst is equal to
 *   packet, the memory pointed to by packet is overwritten.  The memory
 *   pointed to by packet and by dst must not overlap otherwise.
 * @param dst_len Length, in bytes, of destination memory for the result
 *   packet.
 * @param seq_num The sequence number, if any, to assign to this packet.
 */
typedef int ipsec_process_packet_start_func(gxio_mica_context_t* mica_context,
                                            void* op_sa,
                                            void* packet, int packet_len,
                                            void* dst, int dst_len,
                                            int seq_num);

/** 
 * Data structure to describe a MiCA queue operation.  The application must
 * allocate and maintain these descriptors.  When operations are sent to the
 * queue, the queue retains only a pointer to the descriptor.
 */
typedef struct {

  /** The Security Association for this operation. */
  void* sa;

  /** The source address of the packet to be processed. */
  void* src;

  /** The address of the destination for the result of the packet processing. */
  void* dst;

  /** Pointer to the function to start the packet processing. */
  ipsec_process_packet_start_func* packet_start_func;

  /** The length of the source packet. */
  int src_packet_len;

  /** The length of the provided destination memory. */
  int dst_packet_len;

  /** The sequence number, if any, to be assigned to the packet. */
  uint32_t seq_num;
} ipsec_queue_desc_t;

/** Data structure for a queue element.
 */
typedef struct {
  /** Pointer to the descriptor for the  operation. */
  ipsec_queue_desc_t* desc;

  /** Flags to indicate the state of the processing of this element. */
  int flags;
} ipsec_queue_desc_elem_t;

/** Data structure for the MiCA queue.
 */
typedef struct {
  /** The MiCA context that processes this queue. */
  gxio_mica_context_t* mica_context;

  /** The index of next element ready to be sent to the hardware. */
  uint32_t head_idx;

  /** The index of next completed operation to take from queue */
  uint32_t tail_idx;

  /** High 32 bits are a count of available egress command credits,
   * low 32 bits are the next command index.
   */
  uint64_t credits_and_next_index;

  /** Number of elements that have been processed. */
  uint64_t num_completed;

  /** The number of elements in the queue. */
  uint32_t num_elems;

  /** Pointer to actual queue elements. The memory for this is provided
   * at queue initialization time.
   */
  ipsec_queue_desc_elem_t* elem;
} ipsec_queue_t;


/** Initialize a queue for a MiCA context.  
 *
 * @param queue Queue to be initialized.
 * @param mica_context The MiCA context that this queue feeds.
 * @param num_elems Number of elements in queue.
 * @return 0 on success, negative error code on failure.
 */
int
queue_init(ipsec_queue_t* queue, gxio_mica_context_t* mica_context,
           int num_elems);


/** Reserve slots for MiCA commands, if possible.
 *
 * Use queue_put_at() to actually populate the slots.
 *
 * @param queue A queue initialized via queue_init().
 * @param num Number of slots to reserve.
 * @return The first reserved slot, or a negative error code.
 */
int64_t
queue_try_reserve(ipsec_queue_t* queue, int num);


/** Reserve slots for MiCA commands.  This function blocks until the 
 * specified number of slots becomes available. Generally this function
 * should only be used with the interrupt model of queue processing
 * (the function gxio_mica_cfg_completion_interrupt() should be called to
 * register an interrupt handler to service the queue).
 *
 * Use queue_put_at() to actually populate the slots.
 *
 * @param queue A queue initialized via queue_init().
 * @param num Number of slots to reserve.
 * @return The first reserved slot, or a negative error code.
 */
int64_t
queue_reserve(ipsec_queue_t* queue, int num);


/** Post a MiCA command to a MiCA queue at a given slot.
 *
 * The implementation does not check to make sure that the slots
 * are available.
 *
 * @param queue A queue initialized via queue_init().
 * @param desc MiCA command to be posted.
 * @param slot The slot at which to put the descriptor.
 */
void
queue_put_at(ipsec_queue_t* queue, ipsec_queue_desc_t* desc, int slot);


/** Post a single MiCA command to a MiCA queue.  This function does not
 * return until a slot becomes available and the command has been posted.
 * Generally this function should be used with the interrupt model of 
 * queue processing (the function gxio_mica_cfg_completion_interrupt()
 * should be called to register an interrupt handler to service the queue).
 *
 * @param queue A queue initialized via queue_init().
 * @param desc MiCA command to be posted.
 */
void
queue_put(ipsec_queue_t* queue, ipsec_queue_desc_t* desc);


/** Service the queue.  This should be called from the application body 
 * when the polling model is used, or from the completion handler when 
 * the interrupt model is used.  It must not be called from the application
 * body when the interrupt model is used (the polling and interrupt models
 * must not be mixed).  This function can be called at any time when using
 * the polling model, and needs to be called repeatedly until all packets
 * are processed.
 *
 * @param queue A queue initialized via gxio_mpipe_iqueue_init().
 */
void
queue_service(ipsec_queue_t* queue);


/** Get the next processed packet in a queue, if any.
 *
 * If no completed packets are available, returns a negative error code.
 * Otherwise, copies the address of the next valid packet descriptor into
 * desc, consumes that packet descriptor, and returns zero.
 *
 * @param queue A queue initialized via gxio_mpipe_iqueue_init().
 * @param desc A pointer to the address of the descriptor (which is
 * maintained by the application).
 * @return 0 on success, or -1 if no packets are available.
 */
int
queue_try_get(ipsec_queue_t* queue, ipsec_queue_desc_t** desc);


/** Get the next packet in a queue, blocking if necessary.  Because
 * this function blocks indefinitely, it should be generally be used with
 * the interrupt model of queue processing.
 *
 * Waits for a packet to be available, and then copies the address of
 * the packet descriptor into desc, and consumes that packet descriptor.
 *
 * @param queue A queue initialized via queue_init().
 * @param desc A pointer to the address of the descriptor of the packet
 * (which is maintained by the application).
 * @return 0 on success.
 */
int
queue_get(ipsec_queue_t* queue, ipsec_queue_desc_t** desc);


/** Helper function for debug.
 * @param queue Queue for which to print debug information.
 */
void
debug_dump_queue(ipsec_queue_t* queue);
