/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Bogdan Davidoaia
 *
 * @date 2018-04-26
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Services Services
//! Stack service
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_events.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"
#include "xp/remote/gg_remote.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_StackService GG_StackService;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_STACK_SERVICE_SET_TYPE_METHOD    "stack/set_type"

/**
 * Create a Stack service object.
 *
 * @param service [out] Address of the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_StackService_Create(GG_StackService** service);

/**
 * Destroy a Stack service object.
 *
 * @param self The objet on which the method is invoked.
 */
void GG_StackService_Destroy(GG_StackService* self);

/**
 * Get a reference to the stack service GG_RemoteSmoHandler object
 *
 * @param self The object on which this method is invoked.
 *
 * @return A reference to stack service GG_RemoteSmoHandler object
 */
GG_RemoteSmoHandler* GG_StackService_AsRemoteSmoHandler(GG_StackService* self);

/**
 * Register the Stack service with a remote API shell.
 * This function may only be called from the same thread as the one in which
 * the shell is running.
 *
 * @param self The object on which this method is invoked.
 * @param shell Remote Shell instance on which to register the service.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_StackService_Register(GG_StackService* self, GG_RemoteShell* shell);

/**
 * Set the type of stack that should be built by the Stack service upon the next connection.
 * NOTE: this does not immediately build/rebuild a stack, it is merely setting
 * the intent for what should be built when the next opportunity to build a stack
 * happens.
 *
 * @param stack_type Type of stack that should be built
 * @param service Service that should be attached to the stack when built
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_StackService_SetType(GG_StackService* self,
                                  const char*      stack_type,
                                  const char*      service);

/**
 * Get the type of the stack that should be built.
 * NOTE: if the type hasn't been set with a call to GG_StackService_SetType,
 * this method will return a default type
 *
 * @return The stack type stored in the Stack service object
 */
const char* GG_StackService_GetStackType(GG_StackService* self);

/**
 * Get the type of the service that should be attached to the stack.
 * NOTE: if the type hasn't been set with a call to GG_StackService_SetType,
 * this method will return a default type
 *
 * @return The service type stored in the Stack service object
 */
const char* GG_StackService_GetServiceType(GG_StackService* self);

//!@}

#if defined(__cplusplus)
}
#endif
