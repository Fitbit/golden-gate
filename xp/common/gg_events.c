/**
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-03-16
 *
 * @details
 *
 * General purpose events
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "gg_events.h"
#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_EventListener_OnEvent(GG_EventListener* self, const GG_Event* event)
{
    GG_ASSERT(self);

    GG_INTERFACE(self)->OnEvent(self, event);
}

//----------------------------------------------------------------------
void
GG_EventEmitter_SetListener(GG_EventEmitter* self, GG_EventListener* listener)
{
    GG_ASSERT(self);

    GG_INTERFACE(self)->SetListener(self, listener);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_EventListenerBase_SetListener(GG_EventEmitter* _self, GG_EventListener* listener)
{
    GG_EventEmitterBase* self = GG_SELF(GG_EventEmitterBase, GG_EventEmitter);

    self->listener = listener;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_EventEmitterBase, GG_EventEmitter)
{
    .SetListener = GG_EventListenerBase_SetListener
};

//----------------------------------------------------------------------
void
GG_EventEmitterBase_Init(GG_EventEmitterBase* self)
{
    self->listener = NULL;
    GG_SET_INTERFACE(self, GG_EventEmitterBase, GG_EventEmitter);
}
