/**
 * @file
 * @brief General purpose events
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-01-02
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

//! @addtogroup Events
//! General purpose events
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Base type for events.
 */
typedef struct {
    uint32_t type;   ///< Type of the event. [usually a constant constructed with #GG_4CC()]
    void*    source; ///< Source of the event. May be NULL.
} GG_Event;

//----------------------------------------------------------------------
//! Interface implemented by objects that listen for events.
//!
//! @interface GG_EventListener
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_EventListener) {
    void (*OnEvent)(GG_EventListener* self, const GG_Event* event);
};

void GG_EventListener_OnEvent(GG_EventListener* self, const GG_Event* event);

//----------------------------------------------------------------------
//! Interface implemented by objects that emit events.
//!
//! @interface GG_EventEmitter
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_EventEmitter) {
    void (*SetListener)(GG_EventEmitter* self, GG_EventListener* listener);
};

void GG_EventEmitter_SetListener(GG_EventEmitter* self, GG_EventListener* listener);

/**
 * Base implementation for event emitters.
 */
typedef struct {
    GG_IMPLEMENTS(GG_EventEmitter);

    GG_EventListener* listener;
} GG_EventEmitterBase;

/**
 * Initialize the base-class part of an object.
 */
void GG_EventEmitterBase_Init(GG_EventEmitterBase* self);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
