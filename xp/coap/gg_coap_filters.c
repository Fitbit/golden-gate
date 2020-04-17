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
 * @date 2018-08-07
 *
 * @details
 *
 * CoAP library implementation - general purpose filters
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_coap_filters.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_CoapGroupRequestFilter {
    GG_IMPLEMENTS(GG_CoapRequestFilter);

    uint8_t group; // the current group of handlers allowed to handle requests

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_CoapGroupRequestFilter_FilterRequest(GG_CoapRequestFilter* _self,
                                        GG_CoapEndpoint*      endpoint,
                                        uint32_t              handler_flags,
                                        const GG_CoapMessage* request,
                                        GG_CoapMessage**      response)
{
    GG_CoapGroupRequestFilter* self = GG_SELF(GG_CoapGroupRequestFilter, GG_CoapRequestFilter);
    GG_COMPILER_UNUSED(endpoint);
    GG_COMPILER_UNUSED(request);
    GG_COMPILER_UNUSED(response);

    // check if the handler is part of the group, based on its flags
    uint32_t group_mask = GG_COAP_REQUEST_HANDLER_FLAG_GROUP(self->group);
    if (self->group == 0 || (group_mask & handler_flags)) {
        return GG_SUCCESS;
    } else {
        return GG_COAP_MESSAGE_CODE_UNAUTHORIZED;
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapGroupRequestFilter, GG_CoapRequestFilter) {
    .FilterRequest = GG_CoapGroupRequestFilter_FilterRequest
};

//----------------------------------------------------------------------
GG_Result
GG_CoapGroupRequestFilter_Create(GG_CoapGroupRequestFilter** filter)
{
    // allocate a new object
    GG_CoapGroupRequestFilter* self = GG_AllocateZeroMemory(sizeof(GG_CoapGroupRequestFilter));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup interfaces
    GG_SET_INTERFACE(self, GG_CoapGroupRequestFilter, GG_CoapRequestFilter);

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *filter = self;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_CoapGroupRequestFilter_Destroy(GG_CoapGroupRequestFilter* self)
{
    if (self == NULL) return;

    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_CoapRequestFilter*
GG_CoapGroupRequestFilter_AsCoapRequestFilter(GG_CoapGroupRequestFilter* self)
{
    return GG_CAST(self, GG_CoapRequestFilter);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapGroupRequestFilter_SetGroup(GG_CoapGroupRequestFilter* self, uint8_t group)
{
    // check that the group number is acceptable
    if (group > GG_COAP_GROUP_REQUEST_FILTER_MAX_GROUP) {
        return GG_ERROR_OUT_OF_RANGE;
    }

    self->group = group;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
uint8_t
GG_CoapGroupRequestFilter_GetGroup(GG_CoapGroupRequestFilter* self)
{
    return self->group;
}
