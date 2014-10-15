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

#include <arch/interrupts.h>
#include <arch/spr_def.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gxio/mica.h>

#include "queue.h"

#if 0
#define TRACE(...) printf("Queue : " __VA_ARGS__)
#else
#define TRACE(...)
#endif

/** The credit counter lives in the high 32 bits of the
 * credits_and_next_index variable.
 */
#define QUEUE_CREDIT_SHIFT 32

/** The index is in the low 16 bits of the credits_and_next_index variable. */
#define QUEUE_INDEX_MASK ((1 << 16) - 1)

/** Flags to mark the state of the descriptor. */
#define FLAG_READY     1
#define FLAG_PENDING   2
#define FLAG_COMPLETED 4

void
debug_dump_queue(ipsec_queue_t* queue)
{
  printf("mica_context = %p\n", queue->mica_context);
  printf("head_idx = %d\n", queue->head_idx);
  printf("tail_idx = %d\n", queue->tail_idx);
  printf("credits_and_next_index = 0x%016llx\n",
         (long long)(queue->credits_and_next_index));
  printf("credits = 0x%llx\n",
         (long long)(queue->credits_and_next_index) >> QUEUE_CREDIT_SHIFT);
  printf("next_index = 0x%llx\n",
         (long long)(queue->credits_and_next_index) & QUEUE_INDEX_MASK);
  printf("num_completed = 0x%016llx\n", (long long)(queue->num_completed));
  printf("num_elems = %d\n", queue->num_elems);
  for (int i = 0; i < queue->num_elems; i++)
  {
    ipsec_queue_desc_t* desc = queue->elem[i].desc;
    printf("elem[%d]:\n", i);
    printf("  desc = (%p):\n", desc);
    printf("  sa = %p\n", desc->sa);
    printf("  src = %p\n", desc->src);
    printf("  dst = %p\n", desc->dst);
    printf("  packet_start_func = %p\n", desc->packet_start_func);
    printf("  src_packet_len = %d\n", desc->src_packet_len);
    printf("  dst_packet_len = %d\n", desc->dst_packet_len);
    printf("  seq_num = %d\n", desc->seq_num);
    int flags = queue->elem[i].flags;
    printf("  flags = 0x%08x\n", flags);
    printf("  ready = %d\n", flags & FLAG_READY);
    printf("  pending = %d\n", flags & FLAG_PENDING);
    printf("  completed = %d\n", flags & FLAG_COMPLETED);
  }
}


int
queue_init(ipsec_queue_t* queue, gxio_mica_context_t* mica_context,
           int num_elems)
{
  if (num_elems > QUEUE_INDEX_MASK)
    return -1;

  int queue_memsize = num_elems * sizeof(ipsec_queue_desc_elem_t);
  memset(queue, 0, sizeof(*queue));
  queue->elem = malloc(queue_memsize);
  if (queue->elem == NULL)
    return -1;
  memset(queue->elem, 0, queue_memsize);
  queue->mica_context = mica_context;
  queue->num_elems = num_elems;

  queue->credits_and_next_index = (uint64_t)num_elems << QUEUE_CREDIT_SHIFT;

  return 0;
}


// Returns the first reserved slot, or a negative error code.
int64_t
queue_try_reserve(ipsec_queue_t* queue, int num)
{
  // Try to reserve 'num' command slots.  We do this by constructing a
  // constant that subtracts N credits and adds N to the index, and using
  // fetchaddgez to only apply it if the credits count doesn't go negative.
  int64_t modifier = (((int64_t)(-num)) << QUEUE_CREDIT_SHIFT) | num;

  int64_t old = __insn_fetchaddgez(&queue->credits_and_next_index,
                                   modifier);

  if (old + modifier < 0)
  {
    TRACE("try reserve failed to reserve %d slots:\n"
          "credits_and_next_index = 0x%lx, old = 0x%lx, modifier = 0x%lx\n",
          num, queue->credits_and_next_index, old, modifier);
    return -1;
  }

  // Compute the value for "slot" which will correspond to the
  // eventual value of "num_completed".  We combine the low 24
  // bits of "old" with the high 40 bits of "num_completed", and
  // if the result is LESS than "num_completed", then we handle
  // wrapping by adding "1 << 24".  TODO: As a future optimization,
  // whenever "num_completed" is modified, we could store the high
  // 41 bits of "num_completed" in a separate field, but only when
  // they change, and use it instead of "num_completed" above.
  // This will reduce the chance of "inval storms".  TODO: As a future
  // optimization, we could make a version of this function that simply
  // returns "old & 0xffffff", which is "good enough" for many uses.
  uint64_t complete = *((volatile uint64_t*)&queue->num_completed);
  uint64_t slot = (complete & 0xffffffffff000000) | (old & 0xffffff);
  if (slot < complete)
    slot += 0x1000000;

  // If any of our indexes mod 256 were equivalent to 0, 
  // make sure the index doesn't overflow into the credits.
  if (((slot + num) & 0xff) < num)
  {
    *(((uint8_t *)&queue->credits_and_next_index) + 3) = 0;
  }    

  return slot % queue->num_elems;
}

// NOTE: this will block, waiting for slots in the queue.  The only way it
// would ever make sense to use this is either if we're using interrupts
// to service the queue, or servicing the queue from another core.
int64_t
queue_reserve(ipsec_queue_t* queue, int num)
{
  if (num > queue->num_elems)
    return -1;

  int64_t slot;
  while ((slot = queue_try_reserve(queue, num)) < 0)
    ;
  return slot;
}


void
queue_put_at(ipsec_queue_t* queue, ipsec_queue_desc_t* desc, int slot)
{
  queue->elem[slot].desc = desc;
  __insn_mf();
  queue->elem[slot].flags = FLAG_READY;
}

void
queue_put(ipsec_queue_t* queue, ipsec_queue_desc_t* desc)
{
  queue_put_at(queue, desc, queue_reserve(queue, 1));
  __insn_mf();

  // FIXME: If this is the only element on the queue, and if this is only
  // called from the tile which owns this context queue, we can send it
  // straight to hardware without a potential conflict.

  // Due to the restriction of using this on only one tile, it wouldn't
  // make sense to use this function unless you were using the interrupt
  // model, so if the hardware isn't busy we mask the interrupt and
  // call the service routine.
  if (!gxio_mica_is_busy(queue->mica_context))
  {
    __insn_mtspr(SPR_INTERRUPT_MASK_SET_0, 1ULL << INT_IPI_0);
    queue_service(queue);
    __insn_mtspr(SPR_INTERRUPT_MASK_RESET_0, 1ULL << INT_IPI_0);
  }
}

void
queue_service(ipsec_queue_t* queue)
{
  // Queue servicing must be single-threaded.  This function can either 
  // be called by the event hander on a single tile, or it can be called
  // from the app on a single tile.  It can not be called from multiple
  // tiles, nor can it be called from both the event handler and the 
  // app.
  
  int head_idx = queue->head_idx;

  assert((head_idx >= 0) && (head_idx < queue->num_elems));
  assert(queue->elem);

  ipsec_queue_desc_elem_t* head = &queue->elem[head_idx];
  if ((head->desc == NULL) || !(head->flags & FLAG_READY))
  {
    return;
  }

  if (gxio_mica_is_busy(queue->mica_context))
  {
    return;
  }

  if (head->flags & FLAG_PENDING)
  {
    // An operation has completed.  Mark it as such and advance to the
    // next operation on the queue. 
    // FIXME: consider doing all of these at once using bitfields
    // and atomics
    head->flags = FLAG_COMPLETED;
    queue->num_completed++;

    // Now see if the next element should be sent to the hardware.
    // FIXME: this needs to be atomic if it is to be used from 
    // multiple tiles.
    head_idx++;
    if (head_idx >= queue->num_elems)
      head_idx = 0;
    queue->head_idx = head_idx;

    head = &queue->elem[head_idx];  
  }

  // If the next operation is ready, send it to hardware.
  ipsec_queue_desc_t* desc = head->desc;
  if (desc && (head->flags & FLAG_READY))
  {
    head->flags |= FLAG_PENDING;
    desc->packet_start_func(queue->mica_context, desc->sa,
                            desc->src, desc->src_packet_len,
                            desc->dst, desc->dst_packet_len,
                            desc->seq_num);
  }
}

int
queue_try_get(ipsec_queue_t* queue, ipsec_queue_desc_t** desc)
{
  // Non-blocking.
  // If the most recent element on the queue is completed (completed bit in desc is set),
  // update queue pointers, update the valid bit, and return the pointer.
  // If it's not completed return NULL.

  int tail_idx = queue->tail_idx;

  assert((tail_idx >= 0) && (tail_idx < queue->num_elems));

  ipsec_queue_desc_elem_t* elem = &queue->elem[tail_idx];
  if (elem->desc && (elem->flags & FLAG_COMPLETED))
  {
    queue->elem[tail_idx].desc = NULL;

    // FIXME: this needs to be atomic if it is to be used from multiple tiles
    tail_idx++;
    if (tail_idx >= queue->num_elems)
      tail_idx = 0;
    queue->tail_idx = tail_idx;

    // Increment slot credits.
    __insn_fetchadd(&queue->credits_and_next_index,
                    (1ULL << QUEUE_CREDIT_SHIFT));

    elem->flags = 0;
    *desc = elem->desc;
    elem->desc = NULL;

    return 0;
  }
  else
    return -1;
}

int
queue_get(ipsec_queue_t* queue, ipsec_queue_desc_t** desc)
{
  // Blocking.  Call queue_try_get() until we get something.
  while (queue_try_get(queue, desc))
    ;

  return 0;
}
