/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2018-04-26
 *
 * @details
 * Stack service implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_buffer.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/smo/gg_smo_allocator.h"

#include "gg_stack_service.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

typedef enum {
    GG_StackService_Stack_Type_Gattlink,
    GG_StackService_Stack_Type_Udp,
    GG_StackService_Stack_Type_Dtls,
} GG_StackService_Stack_Type;

typedef enum {
    GG_StackService_Service_Type_Blast,
    GG_StackService_Service_Type_Coap,
} GG_StackService_Service_Type;

/**
 * Stack service main object
 */
struct GG_StackService {
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    GG_StackService_Stack_Type stack_type;
    GG_StackService_Service_Type service_type;

    GG_THREAD_GUARD_ENABLE_BINDING
};

typedef struct {
    GG_StackService* self;
} GG_StackServiceDeinitInvokeArgs;

typedef struct {
    GG_StackService* self;
} GG_StackServiceStartInvokeArgs;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static const char* stack_type_str[] = {
    "gattlink",
    "udp",
    "dtls",
};

static const char* service_type_str[] = {
    "blast",
    "coap",
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
// Shared RPC request handler for all the methods
//----------------------------------------------------------------------
static GG_Result
GG_StackService_HandleRequest(GG_RemoteSmoHandler* _self,
                              const char*          request_method,
                              Fb_Smo*              request_params,
                              GG_JsonRpcErrorCode* rpc_error_code,
                              Fb_Smo**             rpc_result)
{
    GG_StackService* self = GG_SELF(GG_StackService, GG_RemoteSmoHandler);

    GG_COMPILER_UNUSED(rpc_error_code);
    GG_COMPILER_UNUSED(rpc_result);

    if (!strcmp(request_method, GG_STACK_SERVICE_SET_TYPE_METHOD)) {
        // extract the parameter objects
        Fb_Smo* stack_type_p = Fb_Smo_GetChildByName(request_params, "stack_type");
        Fb_Smo* service_p    = Fb_Smo_GetChildByName(request_params, "service");

        // check that we have the required parameters
        if (stack_type_p == NULL) {
            return GG_ERROR_INVALID_PARAMETERS;
        }

        // convert the parameter objects
        const char* stack_type = Fb_Smo_GetValueAsString(stack_type_p);
        const char* service    = service_p ? Fb_Smo_GetValueAsString(service_p) : NULL;

        // set the stack and service type
        return GG_StackService_SetType(self, stack_type, service);
    }

    return GG_FAILURE;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_StackService, GG_RemoteSmoHandler) {
    .HandleRequest = GG_StackService_HandleRequest
};

//----------------------------------------------------------------------
// Create a GG_StackService object (called in any thread context,
// typically the GG_RemoteShell thread context)
//----------------------------------------------------------------------
GG_Result
GG_StackService_Create(GG_StackService** service)
{
    // allocate a new object
    *service = NULL;

    GG_StackService* self = (GG_StackService*)GG_AllocateZeroMemory(sizeof(GG_StackService));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    self->stack_type = GG_StackService_Stack_Type_Dtls;
    self->service_type = GG_StackService_Service_Type_Coap;

    // setup interfaces
    GG_SET_INTERFACE(self, GG_StackService, GG_RemoteSmoHandler);

    // bind the object to the thread that createed it
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *service = self;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Destroy a GG_StackService object (called in any thread context,
// typically the GG_RemoteShell thread context)
//----------------------------------------------------------------------
void
GG_StackService_Destroy(GG_StackService* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    // release the memory
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_RemoteSmoHandler*
GG_StackService_AsRemoteSmoHandler(GG_StackService* self)
{
    return GG_CAST(self, GG_RemoteSmoHandler);
}

//----------------------------------------------------------------------
GG_Result
GG_StackService_Register(GG_StackService* self, GG_RemoteShell* shell)
{
    GG_Result result;

    // register RPC methods
    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_STACK_SERVICE_SET_TYPE_METHOD,
                                               GG_StackService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_StackService_SetType(GG_StackService* self, const char* stack_type, const char* service)
{
    int stack_idx;
    int service_idx;

    GG_ASSERT(self);

    stack_idx = -1;
    if (stack_type == NULL) {
        // Use default
        stack_idx = GG_StackService_Stack_Type_Dtls;
    } else {
        for (unsigned int i = 0; i < GG_ARRAY_SIZE(stack_type_str); i++) {
            if (!strcmp(stack_type, stack_type_str[i])) {
                stack_idx = i;
                break;
            }
        }

        if (stack_idx == -1) {
            return GG_ERROR_INVALID_PARAMETERS;
        }
    }

    service_idx = -1;
    if (service == NULL) {
        if (stack_idx == GG_StackService_Stack_Type_Gattlink) {
            service_idx = GG_StackService_Service_Type_Blast;
        } else {
            service_idx = GG_StackService_Service_Type_Coap;
        }
    } else {
        for (unsigned int i = 0; i < GG_ARRAY_SIZE(service_type_str); i++) {
            if (!strcmp(service, service_type_str[i])) {
                service_idx = i;
                break;
            }
        }

        if (service_idx == -1) {
            return GG_ERROR_INVALID_PARAMETERS;
        }
    }

    // Don't allow CoAP over Gattlink
    if (stack_idx == GG_StackService_Stack_Type_Gattlink &&
        service_idx == GG_StackService_Service_Type_Coap) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    self->stack_type = stack_idx;
    self->service_type = service_idx;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
const char*
GG_StackService_GetStackType(GG_StackService* self)
{
    GG_ASSERT(self);

    return stack_type_str[self->stack_type];
}

//----------------------------------------------------------------------
const char*
GG_StackService_GetServiceType(GG_StackService* self)
{
    GG_ASSERT(self);

    return service_type_str[self->service_type];
}
