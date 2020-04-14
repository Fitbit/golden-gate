/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-11-27
 *
 * @details
 *
 * MyNewt implementation of the shared queue.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_utils.h"

#include "os/os.h"
#include "os/os_sem.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_SharedQueue {
    unsigned int  max_items;
    struct os_sem enqueue_sem; // for 'can enqueue' waiting
    struct os_sem dequeue_sem; // for 'can dequeue' waiting
    unsigned int  item_count;
    GG_LinkedList items;
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_MYNEWT_QUEUE_DEFAULT_MAX_ITEMS 0xFFFF

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
        return OS_TIMEOUT_NEVER;
    } else {
        uint32_t ticks = 0;
        os_time_ms_to_ticks((uint32_t)(timeout / (uint64_t)GG_NANOSECONDS_PER_MILLISECOND), &ticks);
        return ticks;
    }
}

static GG_Result
MapErrorCode(os_error_t error)
{
    switch (error) {
        case OS_OK:           return GG_SUCCESS;
        case OS_TIMEOUT:      return GG_ERROR_TIMEOUT;
        case OS_INVALID_PARM: return GG_ERROR_INVALID_PARAMETERS;
        default:              return GG_FAILURE;
    }
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Create(unsigned int max_items, GG_SharedQueue** queue)
{
    // check that max_items fits in a uint16_t
    if (max_items > UINT16_MAX) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // defaults
    if (max_items == 0) {
        max_items = GG_MYNEWT_QUEUE_DEFAULT_MAX_ITEMS;
    }

    // allocate a new object
    GG_SharedQueue* self = (GG_SharedQueue*)GG_AllocateZeroMemory(sizeof(GG_SharedQueue));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    os_error_t result;
    result = os_sem_init(&self->enqueue_sem, (uint16_t)max_items);
    if (result != OS_OK) {
        GG_FreeMemory(self);
        return GG_FAILURE;
    }
    result = os_sem_init(&self->dequeue_sem, 0);
    if (result != OS_OK) {
        GG_FreeMemory(self);
        return MapErrorCode(result);
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

    // release the object
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
    os_error_t result = os_sem_pend(&self->enqueue_sem, ConvertTimeout(timeout));
    if (result != OS_OK) {
        return MapErrorCode(result);
    }

    // add to the list
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    GG_LINKED_LIST_APPEND(&self->items, item);
    ++self->item_count;
    OS_EXIT_CRITICAL(sr);

    // release threads waiting to dequeue
    os_sem_release(&self->dequeue_sem);

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
    os_error_t result = os_sem_pend(&self->dequeue_sem, ConvertTimeout(timeout));
    if (result != OS_OK) {
        return MapErrorCode(result);
    }

    // get the next item from the list
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    GG_ASSERT(self->item_count);
    *item = GG_LINKED_LIST_HEAD(&self->items);
    GG_LINKED_LIST_NODE_REMOVE(*item);
    --self->item_count;
    OS_EXIT_CRITICAL(sr);

    // release threads waiting to enqueue
    os_sem_release(&self->enqueue_sem);

    return GG_SUCCESS;
}
