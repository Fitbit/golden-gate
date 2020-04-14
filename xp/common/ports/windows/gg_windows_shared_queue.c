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
#include <windows.h>

#include "xp/common/gg_lists.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_utils.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_SharedQueue {
    unsigned int     max_items;
    CRITICAL_SECTION mutex;
    HANDLE           can_enqueue_event;
    HANDLE           can_dequeue_event;
    unsigned int     item_count;
    GG_LinkedList    items;
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_WINDOWS_QUEUE_DEFAULT_MAX_ITEMS 1024

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
MapErrorCode(DWORD error)
{
    switch (error) {
        case ERROR_SUCCESS: return GG_SUCCESS;
        default:            return GG_FAILURE;
    }
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Create(unsigned int max_items, GG_SharedQueue** queue)
{
    // defaults
    if (max_items == 0) {
        max_items = GG_WINDOWS_QUEUE_DEFAULT_MAX_ITEMS;
    }

    // create an event initially set
    HANDLE can_enqueue_event = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (can_enqueue_event == NULL) {
        return MapErrorCode(GetLastError());
    }

    // create an event intially not set
    HANDLE can_dequeue_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (can_dequeue_event == NULL) {
        CloseHandle(can_enqueue_event);
        return MapErrorCode(GetLastError());
    }

    // allocate a new object
    GG_SharedQueue* self = (GG_SharedQueue*)GG_AllocateZeroMemory(sizeof(GG_SharedQueue));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    self->can_enqueue_event = can_enqueue_event;
    self->can_dequeue_event = can_dequeue_event;
    InitializeCriticalSection(&self->mutex);
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
    if (!self) {
        return;
    }

    // release the resources
    CloseHandle(self->can_enqueue_event);
    CloseHandle(self->can_dequeue_event);
    DeleteCriticalSection(&self->mutex);

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
static GG_Result
WaitForEvent(HANDLE event, GG_Timeout timeout)
{
    DWORD wait_time = timeout == GG_TIMEOUT_INFINITE ?
                                 INFINITE :
                                 (DWORD)(timeout / GG_NANOSECONDS_PER_MILLISECOND);
    DWORD result = WaitForSingleObject(event, wait_time);
    if (result == WAIT_TIMEOUT) {
        return GG_ERROR_TIMEOUT;
    }
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
        return MapErrorCode(GetLastError());
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Enqueue(GG_SharedQueue* self, GG_LinkedListNode* item, GG_Timeout timeout)
{
    GG_ASSERT(self);
    GG_ASSERT(item);

    // lock the mutex that protects the list
    EnterCriticalSection(&self->mutex);

    // check that we have not exceeded the max
    if (self->max_items) {
        while (self->item_count >= self->max_items) {
            // we must wait until some items have been removed

            // reset the condition to indicate that the queue is full
            ResetEvent(self->can_enqueue_event);

            // unlock the mutex so that another thread can dequeue
            LeaveCriticalSection(&self->mutex);

            // wait for the condition to signal that we can enqueue
            GG_Result result = WaitForEvent(self->can_enqueue_event, timeout);
            if (GG_FAILED(result)) {
                return result;
            }

            // relock the mutex so that we can check the list again
            EnterCriticalSection(&self->mutex);
        }
    }

    // add the item to the list
    GG_LINKED_LIST_APPEND(&self->items, item);
    ++self->item_count;

    // wake up the threads waiting to dequeue
    SetEvent(self->can_dequeue_event);

    // unlock the mutex
    LeaveCriticalSection(&self->mutex);

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

    // lock the mutex that protects the list
    EnterCriticalSection(&self->mutex);

    // wait for items if we need to
    if (timeout) {
        while (self->item_count == 0) {
            // no item in the list, wait for one

            // reset the condition to indicate that the queue is empty
            ResetEvent(self->can_dequeue_event);

            // unlock the mutex so that another thread can enqueue
            LeaveCriticalSection(&self->mutex);

            // wait for the condition to signal that we can dequeue
            GG_Result result = WaitForEvent(self->can_dequeue_event, timeout);
            if (GG_FAILED(result)) {
                return result;
            }

            // relock the mutex so that we can check the list again
            EnterCriticalSection(&self->mutex);
        }
    }

    // get the next item from the list, if any
    if (self->item_count) {
        *item = GG_LINKED_LIST_HEAD(&self->items);
        GG_LINKED_LIST_NODE_REMOVE(*item);
        --self->item_count;
    }

    if (self->max_items && (*item != NULL)) {
        // wake up the threads waiting to enqueue
        SetEvent(self->can_enqueue_event);
    }

    // unlock the mutex
    LeaveCriticalSection(&self->mutex);

    return *item ? GG_SUCCESS : GG_ERROR_TIMEOUT;
}
