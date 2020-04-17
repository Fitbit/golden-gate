/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-10-12
 *
 * @details
 *
 * FreeRTOS implementation of the shared queue.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_lists.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_SharedQueue {
    unsigned int max_items;
    struct {
        SemaphoreHandle_t handle;
        StaticSemaphore_t memory;
    } enqueue_sem; // for 'can enqueue' waiting
    struct {
        SemaphoreHandle_t handle;
        StaticSemaphore_t memory;
    } dequeue_sem; // for 'can dequeue' waiting
    struct {
        SemaphoreHandle_t handle;
        StaticSemaphore_t memory;
    } lock; // to protect the list
    unsigned int  item_count;
    GG_LinkedList items;
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_FREERTOS_QUEUE_DEFAULT_MAX_ITEMS 0xFFFF

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
//  Convert a GG_Timeout value into a tick count
//----------------------------------------------------------------------
static uint32_t
ConvertTimeout(GG_Timeout timeout)
{
    if (timeout == GG_TIMEOUT_INFINITE) {
        return portMAX_DELAY;
    } else if (timeout) {
        return (uint32_t)(((timeout * (uint64_t)configTICK_RATE_HZ) + GG_NANOSECONDS_PER_SECOND - 1) /
                          (uint64_t)GG_NANOSECONDS_PER_SECOND);
    } else {
        return 0;
    }
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Create(unsigned int max_items, GG_SharedQueue** queue)
{
    // defaults
    if (max_items == 0) {
        max_items = GG_FREERTOS_QUEUE_DEFAULT_MAX_ITEMS;
    }

    // allocate a new object
    GG_SharedQueue* self = (GG_SharedQueue*)GG_AllocateZeroMemory(sizeof(GG_SharedQueue));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    self->enqueue_sem.handle = xSemaphoreCreateCountingStatic(max_items, max_items, &self->enqueue_sem.memory);
    if (!self->enqueue_sem.handle) {
        GG_FreeMemory(self);
        return GG_FAILURE;
    }
    self->dequeue_sem.handle = xSemaphoreCreateCountingStatic(max_items, 0, &self->dequeue_sem.memory);
    if (!self->dequeue_sem.handle) {
        vSemaphoreDelete(self->enqueue_sem.handle);
        GG_FreeMemory(self);
        return GG_FAILURE;
    }
    self->lock.handle = xSemaphoreCreateMutexStatic(&self->lock.memory);
    if (!self->lock.handle) {
        vSemaphoreDelete(self->enqueue_sem.handle);
        vSemaphoreDelete(self->dequeue_sem.handle);
        GG_FreeMemory(self);
        return GG_FAILURE;
    }

    GG_LINKED_LIST_INIT(&self->items);
    self->max_items = max_items;

    // return the object
    *queue = self;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_SharedQueue_Destroy(GG_SharedQueue* self)
{
    if (!self) return;

    // release the resources
    vSemaphoreDelete(self->enqueue_sem.handle);
    vSemaphoreDelete(self->dequeue_sem.handle);
    vSemaphoreDelete(self->lock.handle);
    GG_FreeMemory(self);
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Stuff(GG_SharedQueue* self, GG_LinkedListNode* item)
{
    // check that we don't exceed the queue length
    if (self->max_items && self->item_count == self->max_items) {
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // enqueue the item without waiting
    return GG_SharedQueue_Enqueue(self, item, 0);
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Enqueue(GG_SharedQueue* self, GG_LinkedListNode* item, GG_Timeout timeout)
{
    GG_ASSERT(self);
    GG_ASSERT(item);

    // wait until we can enqueue
    if (xSemaphoreTake(self->enqueue_sem.handle, ConvertTimeout(timeout)) != pdTRUE) {
        return GG_ERROR_TIMEOUT;
    }

    // add to the list
    xSemaphoreTake(self->lock.handle, portMAX_DELAY);
    GG_LINKED_LIST_APPEND(&self->items, item);
    ++self->item_count;
    xSemaphoreGive(self->lock.handle);

    // release threads waiting to dequeue
    xSemaphoreGive(self->dequeue_sem.handle);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Dequeue(GG_SharedQueue* self, GG_LinkedListNode** item, GG_Timeout timeout)
{
    GG_ASSERT(self);
    GG_ASSERT(item);

    // default return value
    *item = NULL;

    // wait until we can dequeue
    if (xSemaphoreTake(self->dequeue_sem.handle, ConvertTimeout(timeout)) != pdTRUE) {
        return GG_ERROR_TIMEOUT;
    }

    // get the next item from the list
    xSemaphoreTake(self->lock.handle, portMAX_DELAY);
    GG_ASSERT(self->item_count);
    *item = GG_LINKED_LIST_HEAD(&self->items);
    GG_LINKED_LIST_NODE_REMOVE(*item);
    --self->item_count;
    xSemaphoreGive(self->lock.handle);

    // release threads waiting to enqueue
    xSemaphoreGive(self->enqueue_sem.handle);

    return GG_SUCCESS;
}
