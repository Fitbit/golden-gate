/**
 * @file
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2017-09-14
 * @details
 * See header for more information about API
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xp/common/gg_lists.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_timer.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#if !defined(GG_CONFIG_MAX_TIMERS)
#define GG_CONFIG_MAX_TIMERS 32
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Timer {
    GG_LinkedListNode  list_node;  // to link this object in the scheduled/frozen/nursery lists
    GG_TimerScheduler* scheduler;  // the scheduler that created this timer
    GG_TimerListener*  listener;   // listener that will be called when the timer fires
    uint32_t           start_time; // time when the timer was scheduled
    uint32_t           fire_time;  // time at which the timer will fire
};

struct GG_TimerScheduler {
    GG_Timer      timers[GG_CONFIG_MAX_TIMERS];
    GG_LinkedList scheduled; // time-ordered list of scheduled timers
    GG_LinkedList frozen;    // timers that have been created but not scheduled
    GG_LinkedList nursery;   // timers that are waiting to be created
    uint32_t      now;       // current time
};

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/
void
GG_TimerListener_OnTimerFired(GG_TimerListener *self, GG_Timer *timer, uint32_t time_elapsed)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnTimerFired(self, timer, time_elapsed);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_TimerScheduler_Create(GG_TimerScheduler** scheduler)
{
    GG_ASSERT(scheduler);

    // allocate a new object
    GG_TimerScheduler* self = (GG_TimerScheduler*)GG_AllocateZeroMemory(sizeof(GG_TimerScheduler));
    if (self == NULL) {
        *scheduler = NULL;
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    GG_LINKED_LIST_INIT(&self->scheduled);
    GG_LINKED_LIST_INIT(&self->frozen);
    GG_LINKED_LIST_INIT(&self->nursery);
    for (unsigned int i = 0; i < GG_CONFIG_MAX_TIMERS; i++) {
        self->timers[i].scheduler = self;
        GG_LINKED_LIST_APPEND(&self->nursery, &self->timers[i].list_node);
    }

    // done
    *scheduler = self;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_TimerScheduler_Destroy(GG_TimerScheduler* self)
{
    if (self == NULL) return;

    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
GG_Result
GG_TimerScheduler_CreateTimer(GG_TimerScheduler* self, GG_Timer** timer)
{
    GG_ASSERT(self);

    // check that we have a timer available
    if (GG_LINKED_LIST_IS_EMPTY(&self->nursery)) {
        *timer = NULL;
        return GG_ERROR_OUT_OF_RESOURCES;
    }

    // return a timer from the nursery and add it to the frozen list
    GG_LinkedListNode* timer_node = GG_LINKED_LIST_HEAD(&self->nursery);
    GG_LINKED_LIST_NODE_REMOVE(timer_node);
    GG_LINKED_LIST_APPEND(&self->frozen, timer_node);
    *timer = GG_LINKED_LIST_ITEM(timer_node, GG_Timer, list_node);
    (*timer)->start_time = 0;
    (*timer)->fire_time  = 0;
    (*timer)->listener   = NULL;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_TimerScheduler_SetTime(GG_TimerScheduler* self, uint32_t now)
{
    GG_ASSERT(self);

    // set the current time
    self->now = now;

    unsigned int fire_count = 0;
    while (!GG_LINKED_LIST_IS_EMPTY(&self->scheduled)) {
        GG_Timer* timer = GG_LINKED_LIST_ITEM(GG_LINKED_LIST_HEAD(&self->scheduled), GG_Timer, list_node);
        if (timer->fire_time > now) {
            // timers past this point don't fire yet
            break;
        }

        // prepare to fire this timer
        GG_ASSERT(timer->listener);
        GG_ASSERT(timer->start_time <= now);
        GG_TimerListener* listener = timer->listener;
        uint32_t          elapsed  = now-timer->start_time;

        // unschedule this timer (do this before notifying the listener, so that the listener
        // can re-schedule or destroy it if it wants to).
        GG_Timer_Unschedule(timer);

        // notify the listener
        GG_TimerListener_OnTimerFired(listener, timer, elapsed);
        
        ++fire_count;
    }

    return (GG_Result)fire_count;
}

//----------------------------------------------------------------------
uint32_t
GG_TimerScheduler_GetTime(GG_TimerScheduler* self)
{
    return self->now;
}

//----------------------------------------------------------------------
uint32_t
GG_TimerScheduler_GetNextScheduledTime(GG_TimerScheduler* self)
{
    GG_ASSERT(self);
    if (GG_LINKED_LIST_IS_EMPTY(&self->scheduled)) {
        return GG_TIMER_NEVER;
    }
    GG_Timer* next_timer = GG_LINKED_LIST_ITEM(GG_LINKED_LIST_HEAD(&self->scheduled), GG_Timer, list_node);
    if (next_timer->fire_time >= self->now) {
        return next_timer->fire_time-self->now;
    } else {
        return 0;
    }
}

//----------------------------------------------------------------------
void
GG_Timer_Destroy(GG_Timer* self)
{
    if (self == NULL) return;

    // cleanup this timer and move it to the nursery
    GG_ASSERT(!GG_LINKED_LIST_NODE_IS_UNLINKED(&self->list_node));
    self->listener   = NULL;
    self->start_time = 0;
    self->fire_time  = 0;
    GG_LINKED_LIST_NODE_REMOVE(&self->list_node);
    GG_LINKED_LIST_APPEND(&self->scheduler->nursery, &self->list_node);
}

//----------------------------------------------------------------------
GG_Result
GG_Timer_Schedule(GG_Timer *self, GG_TimerListener *listener, uint32_t ms_from_now)
{
    GG_ASSERT(self);

    // check parameters
    if (listener == NULL) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // remove this timer from the scheduled or frozen list
    GG_LINKED_LIST_NODE_REMOVE(&self->list_node);

    // update the timer
    GG_TimerScheduler* scheduler = self->scheduler;
    self->start_time = scheduler->now;
    self->fire_time  = scheduler->now + ms_from_now;
    self->listener   = listener;

    // (re)insert the timer into the scheduled list at the proper location
    GG_LinkedListNode* insertion_point = NULL;
    GG_LINKED_LIST_FOREACH(node, &scheduler->scheduled) {
        GG_Timer* timer = GG_LINKED_LIST_ITEM(node, GG_Timer, list_node);
        if (self->fire_time < timer->fire_time) {
            insertion_point = node;
            break;
        }
    }
    if (insertion_point) {
        // insert before the insertion point
        GG_LINKED_LIST_NODE_INSERT_BEFORE(insertion_point, &self->list_node);
    } else {
        // we're last
        GG_LINKED_LIST_APPEND(&scheduler->scheduled, &self->list_node);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
bool
GG_Timer_IsScheduled(GG_Timer *self)
{
    GG_ASSERT(self);
    return self->listener != NULL;
}

//----------------------------------------------------------------------
void
GG_Timer_Unschedule(GG_Timer *self)
{
    GG_ASSERT(self);
    GG_ASSERT(self->scheduler);

    // sanity check
    if (!self->listener) {
        // not scheduled...
        return;
    }

    // remove this from the scheduled list and put it in the frozen list
    GG_LINKED_LIST_NODE_REMOVE(&self->list_node);
    GG_LINKED_LIST_PREPEND(&self->scheduler->frozen, &self->list_node);

    // NULL out the listener to mark that we're not scheduled anymore
    self->listener = NULL;
}

//----------------------------------------------------------------------
uint32_t
GG_Timer_GetRemainingTime(GG_Timer *self)
{
    GG_ASSERT(self);

    // check if the timer is active
    if (!GG_Timer_IsScheduled(self)) {
        return 0;
    }

    return (self->fire_time > self->scheduler->now) ?
           (self->fire_time - self->scheduler->now) : 0;
}
