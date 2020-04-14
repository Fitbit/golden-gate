/**
 * @file
 * @brief Module-wide functions and data
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-20
 *
 * @details
 *
 * General module-wide functions that relate to the global state of the
 * library.
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_results.h"
#include "xp/common/gg_utils.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//! @addtogroup Module Module
//! Module-wide functions and data
//! @{

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Initialize the Golden Gate library.
 *
 * This functions must be called before any of the other library functions
 * can be called.
 * It is normally called just once, but it is safe to call it more than
 * once, provided that it is not called concurrently from different threads.
 * If a submodule fails to initialize before returning, this function will
 * internally call GG_Module_Terminate before returning.
 *
 * @return GG_SUCCESS if the library was initialized, or a negative error code.
 */
GG_Result GG_Module_Initialize(void);

/**
 * Terminate the Golden Gate library.
 *
 * This function must be called when the host application knows it will no longer
 * make any other calls to the library.
 * All the registered termination handlers will be called, once each, in the reverse
 * order in which they were registered.
 * This function should be called from the same thread as the one from which
 * GG_Module_Initialize() was called, and must never be called concurrently from
 * two different threads.
 */
void GG_Module_Terminate(void);

/**
 * Register a termination handler.
 * All registered handlers will be called exactly once each when GG_Module_Terminate() is called.
 *
 * @param handler Handler to register
 * @return GG_SUCCESS if the handler could be registered, or a negative error code.
 */
GG_Result GG_Module_RegisterTerminationHandler(GG_GenericCallbackHandler* handler);

#ifdef __cplusplus
}
#endif /* __cplusplus */

//! @}
