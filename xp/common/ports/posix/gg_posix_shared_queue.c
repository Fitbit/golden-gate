/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-02
 *
 * @details
 *
 * POSIX implementation of the shared queue.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#include "xp/common/gg_logging.h"
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
    unsigned int    max_items;
    pthread_mutex_t mutex;
    pthread_cond_t  can_push_condition;
    pthread_cond_t  can_pop_condition;
    unsigned int    pushers_waiting_count;
    unsigned int    poppers_waiting_count;
    GG_LinkedList   items;
    unsigned int    item_count;
};

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.shared-queue.posix")

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
ComputeTimeout(struct timespec* when, GG_Timeout timeout)
{
    // get the current time
    struct timeval now;
    if (gettimeofday(&now, NULL)) {
        return;
    }

    // convert to nanoseconds
    uint64_t now_ns = (uint64_t)now.tv_sec  * (uint64_t)GG_NANOSECONDS_PER_SECOND +
                      (uint64_t)now.tv_usec * (uint64_t)GG_NANOSECONDS_PER_MICROSECOND;

    // compute the end time
    uint64_t end_ns = now_ns + timeout;

    // convert to a timespec
    when->tv_sec  = (time_t)(end_ns / GG_NANOSECONDS_PER_SECOND);
    when->tv_nsec = (time_t)(end_ns % GG_NANOSECONDS_PER_SECOND);
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Create(unsigned int max_items, GG_SharedQueue** queue)
{
    // allocate a new object
    GG_SharedQueue* self = (GG_SharedQueue*)GG_AllocateZeroMemory(sizeof(GG_SharedQueue));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    pthread_mutex_init(&self->mutex, NULL);
    pthread_cond_init(&self->can_push_condition, NULL);
    pthread_cond_init(&self->can_pop_condition, NULL);
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
    if (self == NULL) return;

    // destroy resources
    pthread_cond_destroy(&self->can_push_condition);
    pthread_cond_destroy(&self->can_pop_condition);
    pthread_mutex_destroy(&self->mutex);

    // free the memory
    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Stuff(GG_SharedQueue* self, GG_LinkedListNode* item)
{
    // check that we don't exceed the queue length
    if (self->max_items && self->item_count == self->max_items) {
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // add the item to the queue
    GG_LINKED_LIST_APPEND(&self->items, item);
    ++self->item_count;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Enqueue(GG_SharedQueue* self, GG_LinkedListNode* item, GG_Timeout timeout)
{
    GG_ASSERT(self);
    GG_ASSERT(item);

    // setup the timeout
    struct timespec timed;
    if (timeout != GG_TIMEOUT_INFINITE) {
        ComputeTimeout(&timed, timeout);
    }

    // lock the mutex that protects the list
    if (pthread_mutex_lock(&self->mutex)) {
        return GG_ERROR_INTERNAL;
    }

    // check that we have not exceeded the max
    GG_Result result = GG_SUCCESS;
    if (self->max_items) {
        while (self->item_count >= self->max_items) {
            // wait until we can push
            ++self->pushers_waiting_count;
            if (timeout == GG_TIMEOUT_INFINITE) {
                int wait_result = pthread_cond_wait(&self->can_push_condition, &self->mutex);
                if (wait_result) {
                    GG_LOG_SEVERE("pthread_cond_timedwait failed (%d)", wait_result);
                    pthread_mutex_unlock(&self->mutex);
                    return GG_ERROR_ERRNO(wait_result);
                }
                --self->pushers_waiting_count;
            } else {
                int wait_result = pthread_cond_timedwait(&self->can_push_condition,
                                                         &self->mutex,
                                                         &timed);
                --self->pushers_waiting_count;
                if (wait_result) {
                    if (wait_result == ETIMEDOUT) {
                        result = GG_ERROR_TIMEOUT;
                        break;
                    }
                    GG_LOG_SEVERE("pthread_cond_timedwait failed (%d)", wait_result);
                    ++self->pushers_waiting_count;
                    pthread_mutex_unlock(&self->mutex);
                    return GG_ERROR_ERRNO(wait_result);
                }
            }
        }
    }

    // add the item to the list
    if (result == GG_SUCCESS) {
        GG_LINKED_LIST_APPEND(&self->items, item);
        ++self->item_count;

        // wake up any thread that may be waiting to pop
        if (self->poppers_waiting_count) {
            pthread_cond_broadcast(&self->can_pop_condition);
        }
    }

    // unlock the mutex
    pthread_mutex_unlock(&self->mutex);

    return result;
}

//----------------------------------------------------------------------
GG_Result
GG_SharedQueue_Dequeue(GG_SharedQueue* self, GG_LinkedListNode** item, GG_Timeout timeout)
{
    GG_ASSERT(self);
    GG_ASSERT(item);

    // default return value
    *item = NULL;

    // setup the timeout
    struct timespec timed;
    if (timeout != GG_TIMEOUT_INFINITE) {
        ComputeTimeout(&timed, timeout);
    }

    // lock the mutex that protects the list
    if (pthread_mutex_lock(&self->mutex)) {
        return GG_ERROR_INTERNAL;
    }

    // wait for an item if we need to
    if (timeout) {
        while (self->item_count == 0) {
            // no item in the list, wait for one
            ++self->poppers_waiting_count;
            if (timeout == GG_TIMEOUT_INFINITE) {
                GG_LOG_FINE("pthread_cond_wait");
                int wait_result = pthread_cond_wait(&self->can_pop_condition, &self->mutex);
                if (wait_result) {
                    GG_LOG_SEVERE("pthread_cond_timedwait failed (%d)", wait_result);
                    pthread_mutex_unlock(&self->mutex);
                    return GG_ERROR_ERRNO(wait_result);
                }
                --self->poppers_waiting_count;
            } else {
                GG_LOG_FINE("pthread_cond_timedwait");
                int wait_result = pthread_cond_timedwait(&self->can_pop_condition,
                                                         &self->mutex,
                                                         &timed);
                --self->poppers_waiting_count;
                if (wait_result) {
                    if (wait_result == ETIMEDOUT) {
                        break;
                    }
                    GG_LOG_SEVERE("pthread_cond_timedwait failed (%d)", wait_result);
                    ++self->poppers_waiting_count;
                    pthread_mutex_unlock(&self->mutex);
                    return GG_ERROR_ERRNO(wait_result);
                }
            }
        }
    }

    // dequeue an item if there is one
    GG_Result result;
    if (self->item_count) {
        *item = GG_LINKED_LIST_HEAD(&self->items);
        GG_LINKED_LIST_NODE_REMOVE(*item);
        --self->item_count;
        result = GG_SUCCESS;
    } else {
        result = GG_ERROR_TIMEOUT;
    }

    // wake up any thread that my be waiting to push
    if (self->max_items && (result == GG_SUCCESS) && self->pushers_waiting_count) {
        pthread_cond_broadcast(&self->can_push_condition);
    }

    // unlock the mutex
    pthread_mutex_unlock(&self->mutex);

    return result;
}
