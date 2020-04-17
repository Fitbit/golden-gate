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
 * @date 2017-10-17
 *
 * @details
 *
 * Remote API.
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "fb_smo.h"

#include "xp/common/gg_io.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup RemoteAPI Remote API
//! Remote API Shell
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_RemoteShell GG_RemoteShell;
typedef int16_t GG_JsonRpcErrorCode;

/*----------------------------------------------------------------------
|   error codes
+---------------------------------------------------------------------*/
#define GG_ERROR_REMOTE_EXIT (GG_ERROR_BASE_REMOTE - 0)

#define GG_JSON_RPC_ERROR_UNSET                (-32099)
#define GG_JSON_RPC_ERROR_GENERIC_SERVER_ERROR (-32000)
#define GG_JSON_RPC_ERROR_INVALID_JSON         (-32700)
#define GG_JSON_RPC_ERROR_INVALID_REQUEST      (-32600)
#define GG_JSON_RPC_ERROR_METHOD_NOT_FOUND     (-32601)
#define GG_JSON_RPC_ERROR_INVALID_PARAMETERS   (-32602)
#define GG_JSON_RPC_ERROR_INTERNAL             (-32603)

/*----------------------------------------------------------------------
|   GG_RemoteTransport interface
+---------------------------------------------------------------------*/
/**
 * Synchronous transport for a remote shell to communicate binary frames
 * with a peer.
 */
GG_DECLARE_INTERFACE(GG_RemoteTransport) {
    /**
     * Send a frame.
     *
     * @param self The object on which the method is called.
     * @param data The frame to send.
     * @return GG_SUCCESS if the frame was successfully sent, or an error code otherwise.
     */
    GG_Result (*Send)(GG_RemoteTransport* self, GG_Buffer* data);

    /**
     * Receive a frame.
     * This method blocks until a frame is received or an error occurs.
     *
     * @param self The object on which the method is called.
     * @param data Pointer to the variable that will receive a pointer to the received frame.
     * @return GG_SUCCESS if the frame was successfully received, or an error code otherwise.
     */
    GG_Result (*Receive)(GG_RemoteTransport* self, GG_Buffer** data);
};

// thunks
GG_Result GG_RemoteTransport_Send(GG_RemoteTransport* self, GG_Buffer* data);
GG_Result GG_RemoteTransport_Receive(GG_RemoteTransport* self, GG_Buffer** data);

/*----------------------------------------------------------------------
|   GG_RemoteSmoHandler interface
+---------------------------------------------------------------------*/
GG_DECLARE_INTERFACE(GG_RemoteSmoHandler) {
    /**
     * Handle a request.
     *
     * @param self The object on which the method is called.
     * @param request_method Name of the method for which this handler was registered.
     * @param request_params The request parameters, represented as an Fb_Smo object.
     * @param rpc_error_code Pointer to a variable in which the handler COULD set
     *                       a application JSON-RPC error code. If this variable is defined,
     *                       the handler must return a generic GG_FAILURE. If set, the value
     *                       pointed to by this variable must be a custom JSON-RPC error code.
     *                       This means it must be -32000 < *rpc_error_code < 0
     * @param rpc_result Pointer to a variable in which the handler COULD return a result.
     *                   If the method returns GG_SUCCESS, it will be exposed in `result`.
     *                   If the method returns an error code, it will be exposed in `error.data`.
     * @return GG_SUCCESS if the request was handled,
     *         GG_FAILURE if `rpc_error_code` was set,
     *         or a more specific error code otherwise.
     */
    GG_Result (*HandleRequest)(GG_RemoteSmoHandler* self,
                               const char*          request_method,
                               Fb_Smo*              request_params,
                               GG_JsonRpcErrorCode* rpc_error_code,
                               Fb_Smo**             rpc_result);
};

// thunks
GG_Result GG_RemoteSmoHandler_HandleRequest(GG_RemoteSmoHandler* self,
                                            const char*          request_method,
                                            Fb_Smo*              request_params,
                                            GG_JsonRpcErrorCode* rpc_error_code,
                                            Fb_Smo**             rpc_result);

/*----------------------------------------------------------------------
|   GG_RemoteCborHandler interface
+---------------------------------------------------------------------*/
GG_DECLARE_INTERFACE(GG_RemoteCborHandler) {
    /**
     * Handle a request.
     *
     * @param self The object on which the method is called.
     * @param request_method Name of the method for which this handler was registered.
     * @param request_params The request parameters, represented as CBOR-encoded object.
     * @param rpc_error_code Pointer to a variable in which the handler COULD set
     *                       a application JSON-RPC error code. If this variable is defined,
     *                       the handler must return a generic GG_FAILURE. If set, the value
     *                       pointed to by this variable must be a custom JSON-RPC error code.
     *                       This means it must be -32000 < *rpc_error_code < 0
     * @param rpc_result Pointer to a variable in which the handler COULD return a result.
     *                   If the method returns GG_SUCCESS, it will be exposed in `result`.
     *                   If the method returns an error code, it will be exposed in `error.data`.
     * @return GG_SUCCESS if the request was handled,
     *         GG_FAILURE if `rpc_error_code` was set,
     *         or a more specific error code otherwise.
     */
    GG_Result (*HandleRequest)(GG_RemoteCborHandler* self,
                               const char*           request_method,
                               GG_Buffer*            request_params,
                               GG_JsonRpcErrorCode*  rpc_error_code,
                               GG_Buffer**           rpc_result);
};

// thunks
GG_Result GG_RemoteCborHandler_HandleRequest(GG_RemoteCborHandler* self,
                                             const char*           request_method,
                                             GG_Buffer*            request_params,
                                             GG_JsonRpcErrorCode*  rpc_error_code,
                                             GG_Buffer**           rpc_result);

/*----------------------------------------------------------------------
|   GG_RemoteShellHandlerItem
+---------------------------------------------------------------------*/
typedef struct {
    GG_LinkedListNode         list_node;    ///< List node for linking multiple handlers
    const char*               name;         ///< Name of handler item
    bool                      auto_release; ///< Flag to indicate if memory allocated for this
                                            ///< struct needs to be freed when it is unregisterd
    union {
        GG_RemoteSmoHandler*  smo;          ///< Smo Handler
        GG_RemoteCborHandler* cbor;         ///< Cbor Handler
    } handler;
} GG_RemoteShellHandlerItem;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Obtain the default remote shell object.
 *
 * @param shell pointer to a variable in which the remote shell object will be returned.
 * @return GG_SUCCESS if the default shell object could be returned, or an error code otherwise.
 */
GG_Result GG_RemoteShell_GetDefault(GG_RemoteShell** shell);

/**
 * Create a new remote shell object.
 *
 * @param transport Transport object used by the shell to send and receive data frames.
 * @param shell Pointer to a variable in which the remote shell object will be returned.
 * @return GG_SUCCESS if the default shell object could be returned, or an error code otherwise.
 */
GG_Result GG_RemoteShell_Create(GG_RemoteTransport* transport, GG_RemoteShell** shell);

/**
 * Destroy a remote shell object.
 *
 * @param self The object on which the method is called.
 */
void GG_RemoteShell_Destroy(GG_RemoteShell* self);

/**
 * Unregister a request handler from a remote shell.
 * Use this method to unregister handlers that handle requests represented by CBOR-encoded objects.
 *
 * @param self The object on which the method is called.
 * @param name Name of the handler. This must match the RPC method name that this handler can handle.
 * @param handler Pointer to handler object to be unregistered.
 * @return GG_SUCCESS if the handler could be unregistered, or an error code otherwise.
 */
GG_Result
GG_RemoteShell_UnregisterSmoHandler(GG_RemoteShell*      self,
                                    const char*          name,
                                    GG_RemoteSmoHandler* handler);

/**
 * Unregister a request handler from a remote shell.
 * Use this method to unregister handlers that handle requests represented by Fb_Smo objects.
 *
 * @param self The object on which the method is called.
 * @param name Name of the handler. This must match the RPC method name that this handler can handle.
 * @param handler Pointer to handler object to be unregistered.
 * @return GG_SUCCESS if the handler could be unregistered, or an error code otherwise.
 */
GG_Result
GG_RemoteShell_UnregisterCborHandler(GG_RemoteShell*      self,
                                    const char*           name,
                                    GG_RemoteCborHandler* handler);

/**
 * Register a request handler with a remote shell.
 * Use this method to register handlers that handle requests represented by Fb_Smo objects.
 *
 * NOTE: it is legal to call this method from a thread other than the one in which the shell
 * loop is running.
 * NOTE: this method makes an internal copy of the name parameter
 *
 * @param self The object on which the method is called.
 * @param name Name of the handler. This must match the RPC method name that this handler can handle.
 * @param handler Pointer to handler object to be registered.
 * @return GG_SUCCESS if the handler could be registered, or an error code otherwise.
 */
GG_Result GG_RemoteShell_RegisterSmoHandler(GG_RemoteShell* self, const char* name,
                                            GG_RemoteSmoHandler* handler);

/**
 * Register a request handler with a remote shell.
 * Use this method to register handlers that handle requests represented by CBOR-encoded objects.
 *
 * NOTE: it is legal to call this method from a thread other than the one in which the shell
 * loop is running.
 * NOTE: this method makes an internal copy of the name parameter
 *
 * @param self The object on which the method is called.
 * @param name Name of the handler. This must match the RPC method name that this handler can handle.
 * @param handler Pointer to handler object to be registered.
 * @return GG_SUCCESS if the handler could be registered, or an error code otherwise.
 */
GG_Result GG_RemoteShell_RegisterCborHandler(GG_RemoteShell* self, const char* name,
                                             GG_RemoteCborHandler* handler);

/**
 * Register a request handler item with a remote shell.
 *
 * This method is a variant of GG_RemoteShell_RegisterSmoHandler which doesn't do any
 * dynamic memory allocation.
 *
 * NOTE: it is legal to call this method from a thread other than the one in which
 * the shell loop is running.
 * NOTE: this method doesn't make an internal copy of the name parameter, so it must remain
 * unchanged for as long as the handler is registered.
 * NONE: only item->handler.smo should be filled in, as the other fields will be overwritten
 * by this method.
 *
 * @param self The object on which the method is called.
 * @param name Name of the handler item. This must match the RPC method name
 * that this handler item can handle.
 * @param item Pointer to handler item object to be registered.
 * @return GG_SUCCESS if the handler could be registered, or an error code otherwise.
 */
GG_Result GG_RemoteShell_RegisterSmoHandlerItem(GG_RemoteShell* self, const char* name,
                                                GG_RemoteShellHandlerItem* item);
/**

 * Register a request handler item with a remote shell.
 *
 * This method is a variant of GG_RemoteShell_RegisterCborHandler which doesn't do any
 * dynamic memory allocation.
 *
 * NOTE: it is legal to call this method from a thread other than the one in which
 * the shell loop is running.
 * NOTE: this method doesn't make an internal copy of the name parameter, so it must remain
 * unchanged for as long as the handler is registered.
 * NONE: only item->handler.cbor should be filled in, as the other fields will be overwritten
 * by this method.
 *
 * @param self The object on which the method is called.
 * @param name Name of the handler item. This must match the RPC method name
 * that this handler item can handle.
 * @param item Pointer to handler item object to be registered.
 * @return GG_SUCCESS if the handler could be registered, or an error code otherwise.
 */
GG_Result GG_RemoteShell_RegisterCborHandlerItem(GG_RemoteShell* self, const char *name,
                                                 GG_RemoteShellHandlerItem* item);

/**
 * Process an JSON-RPC request and generate a JSON-RPC response.
 *
 * @param self The object on which the method is called.
 * @param cbor_request CBOR-encoded JSON-RPC request
 * @param cbor_response Pointer to the variable where the CBOR-encoded JSON-RPC response will be returned.
 *
 * @return GG_SUCCESS if the RPC request could be processed, or a negative error code.
 */
GG_Result
GG_RemoteShell_ProcessRequest(GG_RemoteShell* self, GG_Buffer* cbor_request, GG_Buffer** cbor_response);

/**
 * Run a shell in a loop until its transport is disconnected or some fatal error occurs.
 *
 * @param self The object on which the method is called.
 * @return GG_SUCCESS if the shell exited gracefully, or an error code otherwise.
 */
GG_Result GG_RemoteShell_Run(GG_RemoteShell* self);

//! @}

#if defined(__cplusplus)
}
#endif
