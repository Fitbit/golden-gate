/**
 * @file
 * @brief General purpose queues
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-02
*/

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_lists.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Queues
//! General purpose queues
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
//!
//! Queue that may be used concurrently in multiple threads.
//!
typedef struct GG_SharedQueue GG_SharedQueue;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a new GG_SharedQueue object.
 *
 * @param max_items Maximum number of items the queue can hold, or 0 to indicate
 *                  that an unlimited number of items may be queued.
 * @param queue Pointer to a variable where the created object will be returned.
 * @return #GG_SUCCESS if the object could be created, or an error code.
 */
GG_Result GG_SharedQueue_Create(unsigned int max_items, GG_SharedQueue** queue);

/**
 * Destroy the object.
 *
 * @param self THe object on which this method is called.
 */
void GG_SharedQueue_Destroy(GG_SharedQueue* self);

/**
 * Add an item to the queue.
 *
 * This method is *not* thread-safe, so it must not be called concurrently with
 * other methods. This is typically used at initialization time to fill a queue
 * with initial items just after creating it.
 *
 * @param self The object on which this method is called.
 * @param item The item to add to the queue.
 * @return #GG_SUCCESS if the item could be added, or an error code.
 */
GG_Result GG_SharedQueue_Stuff(GG_SharedQueue* self, GG_LinkedListNode* item);

/**
 * Enqueue an item.
 *
 * This function may be called from any thread. It may block until there is space in the queue or a
 * specified timeout expires.
 *
 * @param self The object on which this method is called.
 * @param item The item to enqueue.
 * @param timeout The maximum time the caller is willing to wait, in milliseconds.
 * @return #GG_SUCCESS if the item was enqueued, #GG_ERROR_TIMEOUT if the specified timeout
 *         expired, or an error code.
 */
GG_Result GG_SharedQueue_Enqueue(GG_SharedQueue* self, GG_LinkedListNode* item, GG_Timeout timeout);

/**
 * Dequeue an item.
 *
 * This function may be called from any thread. It may block until there is at least one item available
 * in the queue, or a specified timeout expires.
 *
 * @param self The object on which this method is called.
 * @param item Pointer to a variable in which the dequeued item will be returned, if any.
 * @param timeout The maximum time the caller is willing to wait, in milliseconds.
 * @return #GG_SUCCESS if an item was dequeued, #GG_ERROR_TIMEOUT if the specified timeout
 *         expired, or an error code.
 */
GG_Result GG_SharedQueue_Dequeue(GG_SharedQueue* self, GG_LinkedListNode** item, GG_Timeout timeout);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
