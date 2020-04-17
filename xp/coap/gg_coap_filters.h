/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-08-08
 *
 * @details
 *
 * CoAP library - general purpose filters
 */

#pragma once

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
#include "xp/common/gg_lists.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_threads.h"
#include "xp/coap/gg_coap.h"


//! @addtogroup CoAP CoAP
//! CoAP client and server library
//! @{

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_GROUP_REQUEST_FILTER_MAX_GROUP 4

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Request filter that filters out requests based on the *group* memberships
 * of the handler that is selected to handle a request.
 * Group memberships of a handler are specified at the time the handler
 * is registered, using membership flags as part of the handler flags.
 * There are 4 flag bits reserved for group membership (hence 4 possible
 * groups that a handler can be a member of, in addition to group 0):
 * GG_COAP_REQUEST_HANDLER_FLAG_GROUP_{1,2,3,4}
 *
 * At any point in time, the filter has a *current group* property. When
 * a request is received, the filter will let it pass through if the handler
 * for that request is in the current group, otherwise a GG_COAP_MESSAGE_CODE_FORBIDDEN
 * response is returned.
 *
 * Note that all handlers are automatically implicitly part of group 0, and
 * the default value for the current group is 0 when the filter is first created.
 * This means that by default, all requests will pass through the filter.
 */
typedef struct GG_CoapGroupRequestFilter GG_CoapGroupRequestFilter;

/**
 * Create a new GG_CoapGroupRequestFilter object.
 *
 * @param filter Pointer to the variable where the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapGroupRequestFilter_Create(GG_CoapGroupRequestFilter** filter);

/**
 * Destroy the object.
 */
void GG_CoapGroupRequestFilter_Destroy(GG_CoapGroupRequestFilter* self);

/**
 * Obtain the GG_CoapRequestFilter interface for a GG_CoapGroupRequestFilter object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's GG_CoapRequestFilter interface
 */
GG_CoapRequestFilter* GG_CoapGroupRequestFilter_AsCoapRequestFilter(GG_CoapGroupRequestFilter* self);

/**
 * Set the current group for the filter.
 *
 * @param self The object on which this method is invoked.
 * @param group Group number, between 0 and GG_COAP_GROUP_REQUEST_FILTER_MAX_GROUP
 *
 * @return GG_SUCCESS if the group number was set, or GG_ERROR_OUT_OF_RANGE if the
 * group number is > GG_COAP_GROUP_REQUEST_FILTER_MAX_GROUP.
 */
GG_Result GG_CoapGroupRequestFilter_SetGroup(GG_CoapGroupRequestFilter* self, uint8_t group);

/**
 * Get the current group for the filter.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The filter's current group property.
 */
uint8_t GG_CoapGroupRequestFilter_GetGroup(GG_CoapGroupRequestFilter* self);

//! @}

#if defined(__cplusplus)
}
#endif
