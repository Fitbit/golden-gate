/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-17
 *
 * @details
 *
 * Remote API
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "fb_smo.h"
#include "fb_smo_serialization.h"

#include "xp/common/gg_lists.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/smo/gg_smo_allocator.h"
#include "gg_remote.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.remote")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_REMOTE_SHELL_GET_HANDLERS_METHOD    "shell/get_handlers"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_RemoteShell {
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    GG_RemoteShellHandlerItem get_handlers_item;
    GG_RemoteTransport*       transport;
    GG_Mutex*                 lock;
    GG_LinkedList             cbor_handlers; // TODO: this should use a map instead
    GG_LinkedList             smo_handlers;  // TODO: this should use a map instead

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_RemoteTransport_Send(GG_RemoteTransport* self, GG_Buffer* data)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->Send(self, data);
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteTransport_Receive(GG_RemoteTransport* self, GG_Buffer** data)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->Receive(self, data);
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteSmoHandler_HandleRequest(GG_RemoteSmoHandler* self,
                                  const char*          request_method,
                                  Fb_Smo*              request_params,
                                  GG_JsonRpcErrorCode* rpc_error_code,
                                  Fb_Smo**             rpc_result)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->HandleRequest(self, request_method, request_params, rpc_error_code, rpc_result);
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteCborHandler_HandleRequest(GG_RemoteCborHandler* self,
                                   const char*           request_method,
                                   GG_Buffer*            request_params,
                                   GG_JsonRpcErrorCode*  rpc_error_code,
                                   GG_Buffer**           rpc_result)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->HandleRequest(self, request_method, request_params, rpc_error_code, rpc_result);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_GetDefault(GG_RemoteShell** shell)
{
    *shell = NULL;
    return GG_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
static GG_Result
GG_RemoteShell_HandleRequest(GG_RemoteSmoHandler* _self,
                             const char*          request_method,
                             Fb_Smo*              request_params,
                             GG_JsonRpcErrorCode* rpc_error_code,
                             Fb_Smo**             rpc_result)
{
    GG_RemoteShell* self = GG_SELF(GG_RemoteShell, GG_RemoteSmoHandler);
    GG_COMPILER_UNUSED(request_method);
    GG_COMPILER_UNUSED(request_params);
    GG_COMPILER_UNUSED(rpc_error_code);

    // allocate a result array
    Fb_Smo* handler_list = Fb_Smo_CreateArray(&GG_SmoHeapAllocator);
    if (handler_list == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // add all CBOR handlers
    GG_LINKED_LIST_FOREACH(node, &self->cbor_handlers) {
        GG_RemoteShellHandlerItem* item = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);
        Fb_Smo* name = Fb_Smo_CreateString(&GG_SmoHeapAllocator, item->name, 0);
        if (name) {
            Fb_Smo_AddChild(handler_list, NULL, 0, name);
        }
    }

    // add all SMO handlers
    GG_LINKED_LIST_FOREACH(node, &self->smo_handlers) {
        GG_RemoteShellHandlerItem* item = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);
        Fb_Smo* name = Fb_Smo_CreateString(&GG_SmoHeapAllocator, item->name, 0);
        if (name) {
            Fb_Smo_AddChild(handler_list, NULL, 0, name);
        }
    }

    // done
    *rpc_result = handler_list;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_RemoteShell, GG_RemoteSmoHandler) {
    .HandleRequest = GG_RemoteShell_HandleRequest
};

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_Create(GG_RemoteTransport* transport, GG_RemoteShell** shell)
{
    // allocate a new object
    GG_RemoteShell* self = (GG_RemoteShell*)GG_AllocateZeroMemory(sizeof(GG_RemoteShell));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // init the object
    self->transport = transport;
    GG_LINKED_LIST_INIT(&self->cbor_handlers);
    GG_LINKED_LIST_INIT(&self->smo_handlers);
    GG_Result result = GG_Mutex_Create(&self->lock);
    if (GG_FAILED(result)) {
        GG_FreeMemory(self);
        return result;
    }

    // setup interfaces
    GG_SET_INTERFACE(self, GG_RemoteShell, GG_RemoteSmoHandler);

    // return the object
    *shell = self;

    // register ourself as a handler for the builtin methods
    self->get_handlers_item.handler.smo = GG_CAST(self, GG_RemoteSmoHandler);

    return GG_RemoteShell_RegisterSmoHandlerItem(self,
                                                 GG_REMOTE_SHELL_GET_HANDLERS_METHOD,
                                                 &self->get_handlers_item);
}

//----------------------------------------------------------------------
void
GG_RemoteShell_Destroy(GG_RemoteShell* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    // release the CBOR handlers
    GG_LINKED_LIST_FOREACH(node, &self->cbor_handlers) {
        GG_RemoteShellHandlerItem* item = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);
        if (item->auto_release) {
            GG_FreeMemory(item);
        }
    }

    // release the SMO handlers
    GG_LINKED_LIST_FOREACH(node, &self->smo_handlers) {
        GG_RemoteShellHandlerItem* item = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);
        if (item->auto_release) {
            GG_FreeMemory(item);
        }
    }

    // destroy the lock
    GG_Mutex_Destroy(self->lock);

    // free the object memory
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_UnregisterSmoHandler(GG_RemoteShell*      self,
                                    const char*          name,
                                    GG_RemoteSmoHandler* handler)
{
    GG_ASSERT(self);

    GG_Mutex_Lock(self->lock);
    GG_LINKED_LIST_FOREACH(node, &self->smo_handlers) {
        GG_RemoteShellHandlerItem* handler_node = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);

        bool name_match = !strcmp(name, handler_node->name);
        bool handler_match = !handler || handler_node->handler.smo == handler;

        if (name_match && handler_match) {
            GG_LINKED_LIST_NODE_REMOVE(node);
            if (handler_node->auto_release) {
                GG_FreeMemory(handler_node);
            }
            GG_Mutex_Unlock(self->lock);

            return GG_SUCCESS;
        }
    }
    GG_Mutex_Unlock(self->lock);

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_UnregisterCborHandler(GG_RemoteShell*       self,
                                     const char*           name,
                                     GG_RemoteCborHandler* handler)
{
    GG_ASSERT(self);

    GG_Mutex_Lock(self->lock);
    GG_LINKED_LIST_FOREACH(node, &self->cbor_handlers) {
        GG_RemoteShellHandlerItem* handler_node = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);

        bool name_match = !strcmp(name, handler_node->name);
        bool handler_match = !handler || handler_node->handler.cbor == handler;

        if (name_match && handler_match) {
            GG_LINKED_LIST_NODE_REMOVE(node);
            if (handler_node->auto_release) {
                GG_FreeMemory(handler_node);
            }
            GG_Mutex_Unlock(self->lock);

            return GG_SUCCESS;
        }
    }
    GG_Mutex_Unlock(self->lock);

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
static GG_Result
GG_RemoteShell_RegisterHandlerItem(GG_RemoteShell*            self,
                                   GG_LinkedList*             list,
                                   const char*                name,
                                   GG_RemoteShellHandlerItem* item)
{
    item->name = name;
    item->auto_release = false;

    // add the item to the list
    GG_Mutex_Lock(self->lock);
    GG_LINKED_LIST_APPEND(list, &item->list_node);
    GG_Mutex_Unlock(self->lock);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_RegisterSmoHandlerItem(GG_RemoteShell*            self,
                                      const char*                name,
                                      GG_RemoteShellHandlerItem* item)
{
    return GG_RemoteShell_RegisterHandlerItem(self, &self->smo_handlers, name, item);
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_RegisterCborHandlerItem(GG_RemoteShell*            self,
                                       const char*                name,
                                       GG_RemoteShellHandlerItem* item)
{
    return GG_RemoteShell_RegisterHandlerItem(self, &self->cbor_handlers, name, item);
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_RegisterSmoHandler(GG_RemoteShell* self, const char* name, GG_RemoteSmoHandler* handler)
{
    size_t name_length = strlen(name);
    size_t alloc_size = sizeof(GG_RemoteShellHandlerItem) + name_length + 1;
    char* name_ptr;

    GG_RemoteShellHandlerItem* item =
        (GG_RemoteShellHandlerItem*)GG_AllocateZeroMemory(alloc_size);
    if (item == NULL) return GG_ERROR_OUT_OF_MEMORY;

    name_ptr = (char *)(item + 1);
    memcpy(name_ptr, name, name_length + 1);

    item->handler.smo = handler;

    GG_Result result = GG_RemoteShell_RegisterSmoHandlerItem(self, name_ptr, item);
    if (GG_FAILED(result)) {
        GG_FreeMemory(item);
    }

    item->auto_release = true;

    return result;
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_RegisterCborHandler(GG_RemoteShell* self, const char* name, GG_RemoteCborHandler* handler)
{
    size_t name_length = strlen(name);
    size_t alloc_size = sizeof(GG_RemoteShellHandlerItem) + name_length + 1;
    char *name_ptr;

    GG_RemoteShellHandlerItem* item =
        (GG_RemoteShellHandlerItem*)GG_AllocateZeroMemory(alloc_size);
    if (item == NULL) return GG_ERROR_OUT_OF_MEMORY;

    name_ptr = (char *)(item + 1);
    memcpy(name_ptr, name, name_length + 1);

    item->handler.cbor = handler;

    GG_Result result = GG_RemoteShell_RegisterCborHandlerItem(self, name_ptr, item);
    if (GG_FAILED(result)) {
        GG_FreeMemory(item);
    }

    item->auto_release = true;

    return result;
}

//----------------------------------------------------------------------
static GG_Result
GG_RemoteShell_ParseRequest(GG_Buffer*   payload,
                            Fb_Smo**     request,
                            Fb_Smo**     request_id,
                            const char** request_method,
                            Fb_Smo**     request_params)
{
    // parse the CBOR payload
    *request = NULL;
    int result = Fb_Smo_Deserialize(&GG_SmoHeapAllocator, &GG_SmoHeapAllocator,
                                    FB_SMO_SERIALIZATION_FORMAT_CBOR,
                                    GG_Buffer_GetData(payload),
                                    (unsigned int)GG_Buffer_GetDataSize(payload),
                                    request);

    if (result != FB_SMO_SUCCESS) {
        GG_LOG_FINE("Fb_Smo_Deserialize failed (%d)", result);
        return GG_ERROR_INVALID_FORMAT;
    }

    // get the request ID
    *request_id = Fb_Smo_GetChildByName(*request, "id");
    if (*request_id == NULL) {
        GG_LOG_FINE("request does not have an 'id' property");
    }

    // get the request method
    Fb_Smo* method = Fb_Smo_GetChildByName(*request, "method");
    if (method) {
        *request_method = Fb_Smo_GetValueAsString(method);
    }

    // get the params
    *request_params = Fb_Smo_GetChildByName(*request, "params");

    if (*request_id == NULL ||
        *request_method == NULL ||
        (Fb_Smo_GetType(*request_id) != FB_SMO_TYPE_INTEGER &&
         Fb_Smo_GetType(*request_id) != FB_SMO_TYPE_STRING)) {
        Fb_Smo_Destroy(*request);
        *request = NULL;
        *request_id = NULL;
        *request_method = NULL;
        *request_params = NULL;
        return GG_ERROR_INVALID_SYNTAX;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
SerializeSmoToCbor(Fb_Smo* smo, GG_Buffer** cbor)
{
    unsigned int cbor_size = 0;
    *cbor = NULL;

    // first, measure the space needed
    int smo_result = Fb_Smo_Serialize(smo,
                                      FB_SMO_SERIALIZATION_FORMAT_CBOR,
                                      NULL,
                                      &cbor_size);
    if (smo_result != FB_SMO_SUCCESS) {
        return GG_ERROR_INTERNAL;
    }

    // allocate a buffer for the encoded params
    GG_DynamicBuffer* cbor_buffer = NULL;
    GG_Result result = GG_DynamicBuffer_Create(cbor_size, &cbor_buffer);
    if (GG_FAILED(result)) {
        return GG_ERROR_INTERNAL;
    }

    // serialize
    smo_result = Fb_Smo_Serialize(smo,
                                  FB_SMO_SERIALIZATION_FORMAT_CBOR,
                                  GG_DynamicBuffer_UseData(cbor_buffer),
                                  &cbor_size);
    if (smo_result != FB_SMO_SUCCESS) {
        GG_DynamicBuffer_Release(cbor_buffer);
        return GG_ERROR_INTERNAL;
    }

    // return the buffer
    GG_DynamicBuffer_SetDataSize(cbor_buffer, cbor_size);
    *cbor = GG_DynamicBuffer_AsBuffer(cbor_buffer);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
SynthesizeErrorCodeFromHandlerResult(GG_Result* result, GG_JsonRpcErrorCode* rpc_error_code)
{
    // If the handler returned GG_SUCCESS, it should not have modified `rpc_error_code`
    if (GG_SUCCEEDED(*result)) {
        GG_ASSERT(*rpc_error_code == GG_JSON_RPC_ERROR_UNSET);
    }
    // If the handler modified `rpc_error_code`, enforce "only on GG_FAILURE"
    else if (*rpc_error_code != GG_JSON_RPC_ERROR_UNSET) {
        GG_ASSERT(*result == GG_FAILURE);
        // Enforce only custom error codes when handler returns GG_FAILURE.
        // Standard error codes (apart from INVALID_PARAMETERS) should only be used
        // to denote errors while handling the JSON-RPC message itself. Errors in handling
        // the RPC command should use a custom error code.
        GG_ASSERT(*rpc_error_code > GG_JSON_RPC_ERROR_GENERIC_SERVER_ERROR);
    }
    else if (*result == GG_ERROR_INVALID_PARAMETERS) {
        *rpc_error_code = GG_JSON_RPC_ERROR_INVALID_PARAMETERS;
    }
    else {
        if (*result != GG_FAILURE) {
            GG_LOG_WARNING("Specific error %d will be mapped to generic %d",
                           (int)*result,
                           (int)GG_JSON_RPC_ERROR_GENERIC_SERVER_ERROR);
        }

        *rpc_error_code = GG_JSON_RPC_ERROR_GENERIC_SERVER_ERROR;
    }
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_ProcessRequest(GG_RemoteShell* self, GG_Buffer* cbor_request, GG_Buffer** cbor_response)
{
    GG_JsonRpcErrorCode rpc_error_code = GG_JSON_RPC_ERROR_UNSET;

    // parse the payload
    Fb_Smo*     request        = NULL;
    Fb_Smo*     request_id     = NULL;
    Fb_Smo*     request_params = NULL;
    const char* request_method = NULL;
    GG_Result result = GG_RemoteShell_ParseRequest(cbor_request,
                                                   &request,
                                                   &request_id,
                                                   &request_method,
                                                   &request_params);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("failed to parse CBOR request (%d)", result);

        if (result == GG_ERROR_INVALID_FORMAT) {
            rpc_error_code = GG_JSON_RPC_ERROR_INVALID_JSON;
        } else if (result == GG_ERROR_INVALID_SYNTAX) {
            rpc_error_code = GG_JSON_RPC_ERROR_INVALID_REQUEST;
        } else {
            rpc_error_code = GG_JSON_RPC_ERROR_INTERNAL;
        }
    }

    // find and invoke the matching handler
    Fb_Smo* response_result = NULL;
    if (rpc_error_code == GG_JSON_RPC_ERROR_UNSET) {
        bool handler_found = false;

        // try SMO handlers first
        GG_Mutex_Lock(self->lock);
        GG_LINKED_LIST_FOREACH(node, &self->smo_handlers) {
            GG_RemoteShellHandlerItem* item = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);
            if (!strcmp(request_method, item->name)) {
                GG_Mutex_Unlock(self->lock);
                handler_found = true;

                // invoke the handler
                result = GG_RemoteSmoHandler_HandleRequest(item->handler.smo,
                                                           request_method,
                                                           request_params,
                                                           &rpc_error_code,
                                                           &response_result);

                SynthesizeErrorCodeFromHandlerResult(&result, &rpc_error_code);

                break;
            }
        }

        // try CBOR handlers next
        if (!handler_found) {
            GG_LINKED_LIST_FOREACH(node, &self->cbor_handlers) {
                GG_RemoteShellHandlerItem* item = GG_LINKED_LIST_ITEM(node, GG_RemoteShellHandlerItem, list_node);
                if (!strcmp(request_method, item->name)) {
                    GG_Mutex_Unlock(self->lock);
                    handler_found = true;

                    GG_Buffer* cbor_response_result = NULL;
                    GG_Buffer* cbor_request_params  = NULL;

                    if (request_params) {
                        // serialize the request params
                        result = SerializeSmoToCbor(request_params, &cbor_request_params);
                        if (GG_FAILED(result)) {
                            rpc_error_code = GG_JSON_RPC_ERROR_INTERNAL;
                            break;
                        }
                    }

                    // invoke the handler
                    result = GG_RemoteCborHandler_HandleRequest(item->handler.cbor,
                                                                request_method,
                                                                cbor_request_params,
                                                                &rpc_error_code,
                                                                &cbor_response_result);

                    SynthesizeErrorCodeFromHandlerResult(&result, &rpc_error_code);

                    if (cbor_response_result) {
                        // convert the response payload to an SMO object
                        int smo_result = Fb_Smo_Deserialize(&GG_SmoHeapAllocator,
                                                            &GG_SmoHeapAllocator,
                                                            FB_SMO_SERIALIZATION_FORMAT_CBOR,
                                                            GG_Buffer_GetData(cbor_response_result),
                                                            (unsigned int)
                                                            GG_Buffer_GetDataSize(cbor_response_result),
                                                            &response_result);
                        if (smo_result != FB_SMO_SUCCESS) {
                            rpc_error_code = GG_JSON_RPC_ERROR_INTERNAL;
                        }
                    }

                    // cleanup
                    if (cbor_request_params) {
                        GG_Buffer_Release(cbor_request_params);
                    }
                    if (cbor_response_result) {
                        GG_Buffer_Release(cbor_response_result);
                    }
                    break;
                }
            }
        }

        if (!handler_found) {
            GG_Mutex_Unlock(self->lock);
            rpc_error_code = GG_JSON_RPC_ERROR_METHOD_NOT_FOUND;
        }
    }

    // create the response
    Fb_Smo* response         = Fb_Smo_CreateObject(&GG_SmoHeapAllocator);
    Fb_Smo* response_version = NULL;
    Fb_Smo* response_id      = NULL;

    if (response == NULL) {
        result = GG_ERROR_OUT_OF_MEMORY;
        goto end;
    }

    response_version = Fb_Smo_CreateString(&GG_SmoHeapAllocator, "2.0", 0);
    if (Fb_Smo_AddChild(response, "jsonrpc", 0, response_version) == FB_SMO_SUCCESS) {
        // ownership has been transferred
        response_version = NULL;
    }
    if (request_id) {
        if (Fb_Smo_GetType(request_id) == FB_SMO_TYPE_INTEGER) {
            response_id = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, Fb_Smo_GetValueAsInteger(request_id));
        } else {
            response_id = Fb_Smo_CreateString(&GG_SmoHeapAllocator, Fb_Smo_GetValueAsString(request_id), 0);
        }
    } else {
        // if the request didn't have an ID, respond with a NULL id
        response_id = Fb_Smo_CreateSymbol(&GG_SmoHeapAllocator, FB_SMO_SYMBOL_NULL);
    }
    if (Fb_Smo_AddChild(response, "id", 0, response_id) == FB_SMO_SUCCESS) {
        // ownership has been transferred
        response_id = NULL;
    }
    if (rpc_error_code == GG_JSON_RPC_ERROR_UNSET) {
        // If the handler did not return any payload, we still need to set `result`.
        if (response_result == NULL) {
            response_result = Fb_Smo_CreateSymbol(&GG_SmoHeapAllocator, FB_SMO_SYMBOL_NULL);
        }
        if (Fb_Smo_AddChild(response, "result", 0, response_result) == FB_SMO_SUCCESS) {
            // ownership has been transferred
            response_result = NULL;
        }
    } else {
        Fb_Smo* response_error      = Fb_Smo_CreateObject(&GG_SmoHeapAllocator);
        Fb_Smo* response_error_code = Fb_Smo_CreateInteger(&GG_SmoHeapAllocator, rpc_error_code);
        if (response_error && response_error_code) {
            if (Fb_Smo_AddChild(response_error, "code", 0, response_error_code) == FB_SMO_SUCCESS) {
                // ownsership transfered
                response_error_code = NULL;
            }
            if (response_result) {
                if (Fb_Smo_AddChild(response_error, "data", 0, response_result) == FB_SMO_SUCCESS) {
                    // ownership has been transferred
                    response_result = NULL;
                }
            }
            if (Fb_Smo_AddChild(response, "error", 0, response_error) == FB_SMO_SUCCESS) {
                // ownsership transfered
                response_error = NULL;
            }
        }

        // cleanup
        if (response_error_code) {
            Fb_Smo_Destroy(response_error_code);
        }
        if (response_error) {
            Fb_Smo_Destroy(response_error);
        }
    }

    result = SerializeSmoToCbor(response, cbor_response);

end:
    // cleanup
    if (request) {
        Fb_Smo_Destroy(request);
    }
    if (response_version) {
        Fb_Smo_Destroy(response_version);
    }
    if (response_id) {
        Fb_Smo_Destroy(response_id);
    }
    if (response_result) {
        Fb_Smo_Destroy(response_result);
    }
    if (response) {
        Fb_Smo_Destroy(response);
    }

    return result;
}

//----------------------------------------------------------------------
GG_Result
GG_RemoteShell_Run(GG_RemoteShell* self)
{
    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // run loop
    GG_LOG_INFO("remote shell running");
    for (; /* ever */ ;) {
        // receive a payload
        GG_Buffer* cbor_request = NULL;
        GG_Result result = GG_RemoteTransport_Receive(self->transport, &cbor_request);
        if (GG_FAILED(result)) {
            if (result == GG_ERROR_REMOTE_EXIT) {
                // Shell exit requested
                return GG_SUCCESS;
            }
            GG_LOG_WARNING("GG_RemoteTransport_Receive failed (%d)", result);
            return result;
        }

        // process the request
        GG_Buffer* cbor_response = NULL;
        result = GG_RemoteShell_ProcessRequest(self, cbor_request, &cbor_response);
        GG_Buffer_Release(cbor_request);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("GG_RemoteShell_ProcessRequest failed (%d)", result);
            continue;
        }

        // serialize and send the response
        result = GG_RemoteTransport_Send(self->transport, cbor_response);
        GG_Buffer_Release(cbor_response);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("GG_RemoteTransport_Send failed (%d)", result);
            return result;
        }
    }
}
