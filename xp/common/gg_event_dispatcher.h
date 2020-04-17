/**
 * @file
 * @brief Dispatcher that can have multiple listeners
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Kishore B. Rao
 *
 * @date 2019-02-01
*/

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_events.h"
#include "xp/common/gg_lists.h"
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|  types
+---------------------------------------------------------------------*/

typedef struct GG_EventDispatcher GG_EventDispatcher;

/**
 * Entry in a list of handlers maintained by the event dispather
 */
typedef struct {
    GG_LinkedListNode  list_node;        ///< Linked list node for storing listener context
    GG_EventListener*  listener;         ///< Event listener for this entry
    const uint32_t*    events;           ///< Array of events to listen to, NULL to listen to all events
    size_t             num_events;       ///< Size of events array
    bool               node_is_local;    ///< Flag indicating if this object should be freed
    uint32_t           sequence_number;  ///< Indicates order in which it was added
} GG_EventDispatcher_ListenerNode;

/*----------------------------------------------------------------------
|  Functions
+---------------------------------------------------------------------*/

/**
 * Register as a listener with the dispatcher
 *
 * @param self The object on which this method is invoked
 * @param listener The listener to be registered
 * @param events Array of events the listener will be called back for. NULL to listen to all events
 * @param num_events Number of events in the array
 * @param node Listener Node to store data. If NULL, event dispatcher will allocate and manage
 * memory of the node
 * NOTE: Event Dispatcher will NOT copy over the events. User must ensure events array remains valid
 * A listener can only be registered once. If a listener tries to register again dispatcher will return
 * GG_ERROR_INVALID_PARAMETERS
 * NOTE: Well behaved event listeners should not emit the event they listen to when called back. Listeners
 * that are added during an onEvent callback will be notified only for subsequent events
 * @return Success or Failure
 */
GG_Result GG_EventDispatcher_AddListener(GG_EventDispatcher*                 self,
                                         GG_EventListener*                   listener,
                                         const uint32_t*                     events,
                                         size_t                              num_events,
                                         GG_EventDispatcher_ListenerNode*    node);

/**
 * Remove a listener from the event dispatcher
 *
 * @param self The object on which this method is invoked
 * @param listener The event listener to be removed. Will NOT be called back after removal
 * @return Success or Failure
 */
GG_Result GG_EventDispatcher_RemoveListener(GG_EventDispatcher* self,
                                            GG_EventListener*   listener);


/**
 * Get an Event Dispatcher as an Event Listener
 */
GG_EventListener* GG_EventDispatcher_AsEventListener(GG_EventDispatcher* self);


/**
 * Create an event dispatcher
 */
void GG_EventDispatcher_Create(GG_EventDispatcher** self);

/**
 * Destroy an event dispatcher and clean up memory
 */
void GG_EventDispatcher_Destroy(GG_EventDispatcher* self);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
