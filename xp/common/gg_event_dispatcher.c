/**
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Kishore B. Rao
 *
 * @date 2019-02-04
 *
 * @details
 *
 * General Purpose Event Dispatcher
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "xp/common/gg_event_dispatcher.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_threads.h"

/*----------------------------------------------------------------------
|  types
+---------------------------------------------------------------------*/
/**
 * Concrete implementation for event dispatcher
 */
struct GG_EventDispatcher {
    GG_IMPLEMENTS(GG_EventListener);

    GG_LinkedList listener_contexts;
    uint32_t      sequence_number;
    uint32_t      num_listeners;

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_EventDispatcher_ListenerNode*
GG_EventDispatcher_FindNode(GG_EventDispatcher* self, GG_EventListener* listener)
{
    GG_LINKED_LIST_FOREACH(node, &self->listener_contexts) {
        GG_EventDispatcher_ListenerNode* curr_node = GG_LINKED_LIST_ITEM(node,
                                                                         GG_EventDispatcher_ListenerNode,
                                                                         list_node);
        if (curr_node->listener == listener) { // we have a match
            return curr_node;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
GG_Result
GG_EventDispatcher_AddListener(GG_EventDispatcher*                 self,
                               GG_EventListener*                   listener,
                               const uint32_t*                     events,
                               size_t                              num_events,
                               GG_EventDispatcher_ListenerNode*    listener_node)

{
    // User must provide following information
    GG_ASSERT(listener);
    GG_ASSERT(events);
    GG_ASSERT(num_events);

    GG_THREAD_GUARD_CHECK_BINDING(self);
    GG_EventDispatcher_ListenerNode* found_node = GG_EventDispatcher_FindNode(self, listener);
    if (found_node) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    if (!listener_node) { // Need to allocate
        listener_node = GG_AllocateZeroMemory(sizeof(GG_EventDispatcher_ListenerNode));
        if (!listener_node) {
            return GG_ERROR_OUT_OF_MEMORY;
        }
        listener_node->node_is_local = true;
    } else { // User allocated
        listener_node->node_is_local = false;
    }
    // Set up node variables

    listener_node->listener        = listener;
    listener_node->events          = events;
    listener_node->num_events      = num_events;
    listener_node->sequence_number = self->sequence_number;
    ++self->sequence_number;

    // add the listener_node to the list
    GG_LINKED_LIST_APPEND(&self->listener_contexts, &listener_node->list_node);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_EventDispatcher_RemoveListener(GG_EventDispatcher*   self,
                                  GG_EventListener*     listener)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_EventDispatcher_ListenerNode* node_to_remove = GG_EventDispatcher_FindNode(self, listener);
    if (node_to_remove) {
        GG_LINKED_LIST_NODE_REMOVE(&node_to_remove->list_node);
        if (node_to_remove->node_is_local) {
            GG_FreeMemory(node_to_remove);
        }
        return GG_SUCCESS;
    } else {
        return GG_ERROR_INVALID_PARAMETERS;
    }
}

//----------------------------------------------------------------------
// Helper to notify a listener of an event if it is registered for that event
static void
GG_EventDispatcher_Notify_Listener(GG_EventDispatcher_ListenerNode* curr_node,
                                   const GG_Event*                  event)
{
    const uint32_t* desired_events = curr_node->events;
    if (!desired_events) {
       GG_EventListener_OnEvent(curr_node->listener, event);
       return;
    }
    for (size_t j = 0; j < curr_node->num_events; ++j) {
        if (desired_events[j] == event->type) {
            GG_EventListener_OnEvent(curr_node->listener, event);
            return;
        }
    }

}

//----------------------------------------------------------------------
// Implementation of OnEvent method for EventListener interface
static void
GG_EventDispatcher_OnEvent(GG_EventListener* _self, const GG_Event* event)
{
    GG_EventDispatcher* self = GG_SELF(GG_EventDispatcher, GG_EventListener);

    GG_THREAD_GUARD_CHECK_BINDING(self);

    uint32_t last_notified_node_sequence_number = 0;
    // Store the current max so that new listeners added during event
    // dispatching are ignored until next event
    uint32_t last_node_sequence_number = self->sequence_number;

    bool found_node;
    do { // Loop through listeners and notify
        found_node = false;
        GG_LINKED_LIST_FOREACH(node, &self->listener_contexts) {
            GG_EventDispatcher_ListenerNode* curr_node = GG_LINKED_LIST_ITEM(node,
                                                                             GG_EventDispatcher_ListenerNode,
                                                                             list_node);

            // This is a new listener that was added during iteration, we will
            // not notify it of any events
            if (curr_node->sequence_number >= last_node_sequence_number) {
                break;
            }
            /* Only notify listeners that haven't been visited already
             * Using the sequence number we can ensure that we are only notifying
             * new listeners
            */
            if (curr_node->sequence_number > last_notified_node_sequence_number) {
                // update the sequence number to account for gaps

                last_notified_node_sequence_number = curr_node->sequence_number;
                GG_EventDispatcher_Notify_Listener(curr_node, event);
                // Listeners can modify the list so we must start from the head

                found_node = true;
                break;
            }
        }
        // If we haven't found any matches, that means we have traversed all elts
    } while (found_node);
}

GG_IMPLEMENT_INTERFACE(GG_EventDispatcher, GG_EventListener) {
    .OnEvent = GG_EventDispatcher_OnEvent
};

//----------------------------------------------------------------------
GG_EventListener*
GG_EventDispatcher_AsEventListener(GG_EventDispatcher* self) {
    GG_THREAD_GUARD_BIND(self);
    return GG_CAST(self, GG_EventListener);
}

//----------------------------------------------------------------------
void
GG_EventDispatcher_Create(GG_EventDispatcher** dispatcher)
{
    GG_EventDispatcher* self = (GG_EventDispatcher*)GG_AllocateZeroMemory(sizeof(GG_EventDispatcher));
    GG_THREAD_GUARD_BIND(self);
    GG_LINKED_LIST_INIT(&self->listener_contexts);
    GG_SET_INTERFACE(self, GG_EventDispatcher, GG_EventListener);
    self->sequence_number = 1;
    *dispatcher = self;
}

//----------------------------------------------------------------------
void
GG_EventDispatcher_Destroy(GG_EventDispatcher* self)
{
    if (self == NULL) return;
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // Loop through all listeners and clean up
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->listener_contexts) {
        GG_EventDispatcher_ListenerNode* listener_node = GG_LINKED_LIST_ITEM(node,
                                                                             GG_EventDispatcher_ListenerNode,
                                                                             list_node);
        if (listener_node->node_is_local) {
            GG_FreeMemory(listener_node);
        }
    }

    GG_ClearAndFreeObject(self, 1);
}
