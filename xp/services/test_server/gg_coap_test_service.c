/**
 * @file
 * @copyright
 * Copyright 2018 by Fitbit, Inc., all rights reserved.
 *
 * @brief
 * Fitbit GG CoAP test server implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>

#include "xp/common/gg_memory.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_blockwise.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/remote/gg_remote.h"
#include "xp/loop/gg_loop.h"
#include "xp/remote/gg_remote.h"

#include "gg_coap_test_service.h"
#include "gg_coap_test_service_private.h"

GG_SET_LOCAL_LOGGER("gg.xp.services.testserver")

/*----------------------------------------------------------------------
|   methods
+---------------------------------------------------------------------*/
//! Handler for test/mirror
static GG_CoapRequestHandlerResult
GG_CoapTestService_MirrorRequestHandler_OnRequest(GG_CoapRequestHandler* _self,
                                                  GG_CoapEndpoint*       endpoint,
                                                  const GG_CoapMessage*  request,
                                                  GG_CoapResponder*      responder,
                                                  GG_CoapMessage**       response)
{
    GG_COMPILER_UNUSED(responder);
    GG_CoapTestService* self = GG_SELF(GG_CoapTestService, GG_CoapRequestHandler);

    GG_Result result;
    size_t response_payload_size = 0;
    GG_CoapMessageBlockInfo block1_info;
    GG_CoapMessageBlockInfo block2_info;
    GG_CoapMessageBlockInfo* block1_info_ptr = NULL;
    GG_CoapMessageBlockInfo* block2_info_ptr = NULL;

    uint8_t method = GG_CoapMessage_GetCode(request);
    switch (self->context.state) {
        case TEST_SERVICE_STATE_IDLE:
            // A new request is received.

            // Update incoming payload info
            self->context.received_payload_size = 0;
            self->context.received_payload_crc = 0;
            GG_CoapTestService_UpdateReceivedPayloadInfo(request, &self->context);

            // Parse the queries for response payload size and response code
            self->context.payload_size = 0;
            self->context.response_code = 0;
            GG_CoapTestService_ParseQueryOptions(request, &self->context);

            // Init response payload size with requested size.
            // This can be overwritten for blockwise transfers.
            // There should be no payload sent while receiving blockwise request.
            // Large payload request is handled with blockwise response in chunks.
            response_payload_size = self->context.payload_size;

            // Is this a simple or blockwise request?
            // Check for block info in the request (block1)
            result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK1, &block1_info, 0);
            if (result != GG_ERROR_NO_SUCH_ITEM && result != GG_SUCCESS) {
                GG_LOG_WARNING("Could not parse BLOCK1 option in the request.");
                return GG_COAP_MESSAGE_CODE_BAD_OPTION;
            }
            if (result == GG_SUCCESS) {
                // This is a blockwise request, include block1 option in response
                block1_info_ptr = &block1_info;
                GG_LOG_FINE("block1 %d@%d, more=%s",
                            (int)block1_info.offset,
                            (int)block1_info.size,
                            block1_info.more ? "true" : "false");

                // Keep receiving the blocks before responding.
                if (block1_info.more) {
                    self->context.state = TEST_SERVICE_STATE_BLOCK_RX;
                    response_payload_size = 0;
                }
            }
            if (block1_info_ptr == NULL || block1_info.more == false) {
                // A simple request or last (and first) block
                if (method == GG_COAP_METHOD_PUT || method == GG_COAP_METHOD_POST) {
                    GG_CoapTestService_PopulatePayloadSizeAndCRCOptions(&self->context);
                }
            }

            // Check if response should be simple or blockwise
            // Check request for block2 options for preferred payload size (Optional)
            // Accept any requests
            result = GG_ProcessBlock2Option(request,
                                            &block2_info,
                                            GG_COAP_TEST_SERVER_MAX_PAYLOAD_CHUNK_SIZE,
                                            &response_payload_size,
                                            self->context.payload_size);
            if (GG_FAILED(result)) {
                return result;
            }

            if (block2_info.more) {
                self->context.state = TEST_SERVICE_STATE_BLOCK_TX;
                block2_info_ptr = &block2_info;
            }
            break;

        case TEST_SERVICE_STATE_BLOCK_RX:
            // Collect payload and continue
            GG_CoapTestService_UpdateReceivedPayloadInfo(request, &self->context);

            // Check for block info in the request (block1), mandatory here.
            result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK1, &block1_info, 0);
            if (result == GG_SUCCESS) {
                // This is a blockwise transfer
                GG_LOG_FINE("block1 %d@%d, more=%s",
                            (int)block1_info.offset,
                            (int)block1_info.size,
                            block1_info.more ? "true" : "false");
            } else {
                GG_LOG_WARNING("Expected Block1 option missing.");
                self->context.state = TEST_SERVICE_STATE_IDLE;
                return GG_COAP_MESSAGE_CODE_BAD_OPTION;
            }
            block1_info_ptr = &block1_info;

            // Keep receiving the blocks before responding.
            if (block1_info.more) {
                response_payload_size = 0;
            } else {
                // This is the last block of request.
                // - Add response options
                // - Prepare the response
                if (method == GG_COAP_METHOD_PUT || method == GG_COAP_METHOD_POST) {
                    GG_CoapTestService_PopulatePayloadSizeAndCRCOptions(&self->context);
                }

                // Check if response should be simple or blockwise
                if (self->context.payload_size > GG_COAP_TEST_SERVER_MAX_PAYLOAD_CHUNK_SIZE) {
                    // Start a blockwise response
                    result = GG_ProcessBlock2Option(request,
                                                    &block2_info,
                                                    GG_COAP_TEST_SERVER_MAX_PAYLOAD_CHUNK_SIZE,
                                                    &response_payload_size,
                                                    self->context.payload_size);
                    if (GG_FAILED(result)) {
                        return result;
                    }
                    block2_info_ptr = &block2_info;

                    if (block2_info.more) {
                        self->context.state = TEST_SERVICE_STATE_BLOCK_TX;
                    }
                } else {
                    // The response is not blockwise. Reset state.
                    self->context.state = TEST_SERVICE_STATE_IDLE;

                    // Set the response payload size to requested size
                    response_payload_size = self->context.payload_size;
                }
            }
            break;

        case TEST_SERVICE_STATE_BLOCK_TX:
            // Continue blockwise response
            // Check for block info in the request (block2), it's mandatory here.
            result = GG_ProcessBlock2Option(request, &block2_info, 0,
                                            &response_payload_size, self->context.payload_size);
            if (GG_FAILED(result)) {
                return result;
            }
            block2_info_ptr = &block2_info;

            if (block2_info.more == false) {
                // this is the last block, reset state
                self->context.state = TEST_SERVICE_STATE_IDLE;
            }
            break;
    }
    result = GG_CoapTestService_CreateResponse(&self->context,
                                               endpoint, request,
                                               NULL, response_payload_size,
                                               block1_info_ptr, block2_info_ptr,
                                               response);

    if (GG_FAILED(result)) {
        GG_LOG_WARNING("Resetting server state.");
        self->context.state = TEST_SERVICE_STATE_IDLE;
    }

    return result;
}

//----------------------------------------------------------------------
//! Handler for test/shelf
static GG_CoapRequestHandlerResult
GG_CoapTestService_ShelfRequestHandler_OnRequest(GG_CoapRequestHandler* _self,
                                                 GG_CoapEndpoint*       endpoint,
                                                 const GG_CoapMessage*  request,
                                                 GG_CoapResponder*      responder,
                                                 GG_CoapMessage**       response)
{
    GG_COMPILER_UNUSED(responder);
    GG_CoapTestService* self = GG_SELF(GG_CoapTestService, GG_CoapRequestHandler);

    GG_Result result;
    GG_CoapMessageBlockInfo block1_info;
    GG_CoapMessageBlockInfo block2_info;
    GG_CoapMessageBlockInfo* block1_info_ptr = NULL;
    GG_CoapMessageBlockInfo* block2_info_ptr = NULL;

    GG_CoapTestShelfItem* shelf_item = NULL;
    GG_CoapMessageOptionIterator iterator;

    uint8_t method = GG_CoapMessage_GetCode(request);

    // Get the resource name from the URI
    GG_CoapMessage_InitOptionIterator(request, GG_COAP_MESSAGE_OPTION_URI_PATH, &iterator);
    GG_CoapMessage_StepOptionIterator(request, &iterator); // skip the /test
    GG_CoapMessage_StepOptionIterator(request, &iterator); // skip the /shelf

    if (iterator.option.number == GG_COAP_MESSAGE_OPTION_NONE) {
        // for a POST method create a resource, invalid request otherwise.
        uint8_t* shelf_item_names;
        size_t shelf_item_names_length;
        switch (method) {
            case GG_COAP_METHOD_GET: {
                // Get the list of resources "on the shelf"
                // Payload: comma-separated list of the names of the resources in the "shelf" container
                // 2.05 (Content) or an error code
                result = GG_GetShelfItemNamesCSV(self, &shelf_item_names, &shelf_item_names_length);
                if (GG_FAILED(result)) {
                    return result;
                }
                self->context.response_code = GG_COAP_MESSAGE_CODE_CONTENT;

                size_t chunk_size = shelf_item_names_length;
                size_t offset = 0;
                if (chunk_size) {
                    // Check if need to start a blockwise transfer
                    result = GG_ProcessBlock2Option(request,
                                                    &block2_info,
                                                    GG_COAP_TEST_SERVER_MAX_PAYLOAD_CHUNK_SIZE,
                                                    &chunk_size,
                                                    shelf_item_names_length);
                    if (GG_FAILED(result)) {
                        GG_FreeMemory(shelf_item_names);
                        return result;
                    }
                    if (block2_info.more || block2_info.offset != 0) {
                        block2_info_ptr = &block2_info;
                        offset = block2_info.offset;
                    }
                }

                // Check the requested data is available
                if (offset + chunk_size > shelf_item_names_length) {
                    GG_LOG_WARNING("Requested chunk is out of bound.");
                    GG_FreeMemory(shelf_item_names);
                    return GG_COAP_MESSAGE_CODE_BAD_OPTION;
                }

                result = GG_CoapTestService_CreateResponse(&self->context,
                                                           endpoint, request,
                                                           shelf_item_names + offset,
                                                           chunk_size,
                                                           NULL, block2_info_ptr,
                                                           response);

                // destroy the payload buffer
                GG_FreeMemory(shelf_item_names);
                return result;
            }

            case GG_COAP_METHOD_POST:
                // Put a new unnamed resource on the shelf
                // Response option: Location-Path set to the URI path of the resource that was created
                // 2.01 (Created) if a new resourced was created and stored, or an error code
                if (self->context.active_unnamed_shelf_item == NULL) {
                    result = GG_CoapTestService_CreateUnnamedShelfItem(self, &shelf_item);
                    if (GG_FAILED(result)) {
                        return GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR;
                    }
                    self->context.active_unnamed_shelf_item = shelf_item;
                    GG_LOG_INFO("Created shelf item: %.*s", (int)shelf_item->name_length, shelf_item->name);

                    self->context.response_code = GG_COAP_MESSAGE_CODE_CREATED;
                }
                shelf_item = self->context.active_unnamed_shelf_item;

                // Check for block1 see if there is more payload
                size_t offset = 0;
                result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK1, &block1_info, 0);
                if (result == GG_SUCCESS) {
                    // This is a blockwise request, include block1 option in response
                    block1_info_ptr = &block1_info;
                    GG_LOG_FINE("block1 %d@%d, more=%s",
                                (int)block1_info.offset,
                                (int)block1_info.size,
                                block1_info.more ? "true" : "false");

                    // Check for data offset.
                    // If out of boundary of data buffer, reject.
                    // Replace data chunk even if already exists
                    if (block1_info.offset > GG_DynamicBuffer_GetDataSize(shelf_item->payload_buffer)) {
                        GG_LOG_WARNING("Block1 offset is out of bound");
                        return GG_COAP_MESSAGE_CODE_BAD_OPTION;
                    }
                    offset = block1_info.offset;
                }

                if (offset == 0) { // Reset data
                    result = GG_CoapTestService_SetShelfItemData(shelf_item, request);
                } else {
                    result = GG_CoapTestService_SetShelfItemDataAtOffset(shelf_item, offset, request);
                }
                if (GG_FAILED(result)) {
                    return result;
                }

                if (block1_info_ptr == NULL || block1_info.more == false) {
                    // A simple request or last (and first) block
                    GG_AddShelfURIResponseOption(self);
                    GG_CoapTestService_DumpShelfItems(self);
                    self->context.active_unnamed_shelf_item = NULL;
                }

                return GG_CoapTestService_CreateResponse(&self->context,
                                                            endpoint, request,
                                                            NULL, 0,
                                                            block1_info_ptr, NULL,
                                                            response);

            case GG_COAP_METHOD_DELETE:
                // Remove all resources from the "shelf"
                // 2.02 (Deleted) or an error code
                self->context.response_code = GG_COAP_MESSAGE_CODE_DELETED;
                GG_CoapTestService_DeleteAllShelfItems(self);
                GG_LOG_FINER("Deleted all shelved resources.");
                return GG_CoapTestService_CreateResponse(&self->context,
                                                            endpoint, request,
                                                            NULL, 0,
                                                            NULL, NULL,
                                                            response);
            default:
                return GG_COAP_MESSAGE_CODE_BAD_OPTION;
        }
    } else {
        // Found the resource name
        GG_LOG_INFO("Received request for resource: %.*s",
                    (int)iterator.option.value.string.length,
                    iterator.option.value.string.chars);

        switch (method) {
            case GG_COAP_METHOD_GET: {
                // Send content of the shelf item if exists.
                // 2.05 (Content) if the resource test/shelf/<name> exists
                // 4.04 (Not Found) if it doesn't
                shelf_item = GG_CoapTestService_FindShelfItemWithName(self,
                                                                      (const uint8_t *)
                                                                      iterator.option.value.string.chars,
                                                                      iterator.option.value.string.length);
                if (shelf_item == NULL) {
                    GG_LOG_INFO("Could not find shelf item: %.*s",
                                (int)iterator.option.value.string.length,
                                iterator.option.value.string.chars);

                    return GG_COAP_MESSAGE_CODE_NOT_FOUND;
                }

                GG_LOG_INFO("Found shelf item: %.*s", (int)shelf_item->name_length, shelf_item->name);
                self->context.response_code = GG_COAP_MESSAGE_CODE_CONTENT;
                const uint8_t* payload = GG_DynamicBuffer_GetData(shelf_item->payload_buffer);
                size_t payload_length = GG_DynamicBuffer_GetDataSize(shelf_item->payload_buffer);

                if (payload_length) {
                    // Check if need to start a blockwise transfer
                    result = GG_ProcessBlock2Option(request,
                                                    &block2_info, GG_COAP_TEST_SERVER_MAX_PAYLOAD_CHUNK_SIZE,
                                                    &payload_length, payload_length);
                    if (GG_FAILED(result)) {
                        return result;
                    }
                    if (block2_info.more || block2_info.offset != 0) {
                        block2_info_ptr = &block2_info;
                    }
                }
                return GG_CoapTestService_CreateResponse(&self->context,
                                                            endpoint, request,
                                                            payload, payload_length,
                                                            NULL, block2_info_ptr,
                                                            response);
            }

            case GG_COAP_METHOD_PUT: {
                // Put or update a resource "on the shelf"
                // 2.01 (Created) if the resource didn't already exist,
                // 2.04 (Changed) if the resource already existed,
                // 4.13 (Request Entity Too Large) if the payload is too large to fit in memory
                shelf_item = GG_CoapTestService_FindShelfItemWithName(self,
                                                                      (const uint8_t *)
                                                                      iterator.option.value.string.chars,
                                                                      iterator.option.value.string.length);
                if (shelf_item) {
                    self->context.response_code = GG_COAP_MESSAGE_CODE_CHANGED;
                    GG_LOG_INFO("Found shelf item: %.*s", (int)shelf_item->name_length, shelf_item->name);
                } else {
                    self->context.response_code = GG_COAP_MESSAGE_CODE_CREATED;
                    GG_CoapTestService_CreateShelfItem(self,
                                                       (const uint8_t *)iterator.option.value.string.chars,
                                                       iterator.option.value.string.length,
                                                       &shelf_item);
                    GG_LOG_INFO("Created shelf item: %.*s", (int)shelf_item->name_length, shelf_item->name);
                }

                size_t offset = 0;
                result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK1, &block1_info, 0);
                if (result == GG_SUCCESS) {
                    // This is a blockwise request, include block1 option in response
                    block1_info_ptr = &block1_info;
                    GG_LOG_FINE("block1 %d@%d, more=%s",
                                (int)block1_info.offset,
                                (int)block1_info.size,
                                block1_info.more ? "true" : "false");
                    // Check for data offset.
                    // If out of boundary of data buffer, reject.
                    // Replace data chunk even if already exists
                    if (block1_info.offset > GG_DynamicBuffer_GetDataSize(shelf_item->payload_buffer)) {
                        GG_LOG_WARNING("Block1 offset is out of bound");
                        return GG_COAP_MESSAGE_CODE_BAD_OPTION;
                    }
                    offset = block1_info.offset;
                }

                if (offset == 0) { // Reset data
                    result = GG_CoapTestService_SetShelfItemData(shelf_item, request);
                } else {
                    result = GG_CoapTestService_SetShelfItemDataAtOffset(shelf_item, offset, request);
                }
                if (GG_FAILED(result)) {
                    return result;
                }

                if (block1_info_ptr == NULL || block1_info.more == false) {
                    // A simple request or last (and first) block
                    GG_CoapTestService_DumpShelfItems(self);
                }

                return GG_CoapTestService_CreateResponse(&self->context,
                                                            endpoint, request,
                                                            NULL, 0,
                                                            block1_info_ptr, NULL,
                                                            response);
            }

            case GG_COAP_METHOD_DELETE:
                // Remove a resource from the "shelf"
                // 2.02 (Deleted) if the resource test/shelf/<name> exists
                // 4.04 (Not Found) if it doesn't
                shelf_item = GG_CoapTestService_FindShelfItemWithName(self,
                                                                        (const uint8_t *)
                                                                        iterator.option.value.string.chars,
                                                                        iterator.option.value.string.length);
                if (shelf_item == NULL) {
                    GG_LOG_FINE("Shelf %.*s was not found to be deleted.",
                                (int)iterator.option.value.string.length,
                                iterator.option.value.string.chars);

                    return GG_COAP_MESSAGE_CODE_NOT_FOUND;
                }

                GG_LOG_FINE("Deleting shelf item: %.*s",
                            (int)iterator.option.value.string.length,
                            iterator.option.value.string.chars);

                GG_CoapTestService_DeleteShelfItem(shelf_item);

                self->context.response_code = GG_COAP_MESSAGE_CODE_DELETED;
                return GG_CoapTestService_CreateResponse(&self->context,
                                                            endpoint, request,
                                                            NULL, 0,
                                                            NULL, NULL,
                                                            response);

            default:
                return GG_COAP_MESSAGE_CODE_BAD_OPTION;
        }
    }

    // Should never get here!
    return GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR;
}

//----------------------------------------------------------------------
//! Handler for test/mirror and test/shelf
static GG_CoapRequestHandlerResult
GG_CoapTestService_RequestHandler_OnRequest(GG_CoapRequestHandler*   _self,
                                            GG_CoapEndpoint*         endpoint,
                                            const GG_CoapMessage*    request,
                                            GG_CoapResponder*        responder,
                                            const GG_BufferMetadata* transport_metadata,
                                            GG_CoapMessage**         response)
{
    GG_COMPILER_UNUSED(transport_metadata);
    GG_CoapTestService* self = GG_SELF(GG_CoapTestService, GG_CoapRequestHandler);
    self->context.response_options_count = 0;
    self->context.options_buffer_offset = 0;

    // check for the URI of the request and dispatch corresponding handler
    GG_CoapMessageOptionIterator iterator;
    GG_CoapMessage_InitOptionIterator(request, GG_COAP_MESSAGE_OPTION_URI_PATH, &iterator);
    GG_CoapMessage_StepOptionIterator(request, &iterator); // skip the /test

    if (iterator.option.value.string.chars[0] == 'm') {  // check for /mirror
        return GG_CoapTestService_MirrorRequestHandler_OnRequest(_self, endpoint, request, responder, response);
    } else if (iterator.option.value.string.chars[0] == 's') {  // check for /shelf
        return GG_CoapTestService_ShelfRequestHandler_OnRequest(_self, endpoint, request, responder, response);
    } else { // Should never get here since handler is registered for test/mirror and test/shelf only
        return GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR;
    }
}

GG_IMPLEMENT_INTERFACE(GG_CoapTestService, GG_CoapRequestHandler) {
    .OnRequest = GG_CoapTestService_RequestHandler_OnRequest,
};

//----------------------------------------------------------------------
//! Register CoAP test service handlers
GG_Result GG_CoapTestService_Register(GG_CoapTestService* self)
{
    // Check for NULL pointers
    GG_ASSERT(self);

    GG_Result result;
    result = GG_CoapEndpoint_RegisterRequestHandler(self->endpoint,
                                                    GG_COAP_TEST_SERVICE_MIRROR_URI,
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_DELETE |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_GROUP_1,
                                                    GG_CAST(self, GG_CoapRequestHandler));
    GG_CHECK_SEVERE(result);

    result = GG_CoapEndpoint_RegisterRequestHandler(self->endpoint,
                                                    GG_COAP_TEST_SERVICE_SHELF_URI,
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_PUT |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_DELETE |
                                                    GG_COAP_REQUEST_HANDLER_FLAG_GROUP_1,
                                                    GG_CAST(self, GG_CoapRequestHandler));
    GG_CHECK_SEVERE(result);
    return result;
}

//----------------------------------------------------------------------
//! Unregister CoAP test service handlers
//! @returns GG_SUCCESS. Unregistration only fails if the resource is not registered.
GG_Result GG_CoapTestService_Unregister(GG_CoapTestService* self)
{
    // Check for NULL pointers
    GG_ASSERT(self);

    GG_CoapEndpoint_UnregisterRequestHandler(self->endpoint,
                                             GG_COAP_TEST_SERVICE_MIRROR_URI,
                                             GG_CAST(self, GG_CoapRequestHandler));

    GG_CoapEndpoint_UnregisterRequestHandler(self->endpoint,
                                             GG_COAP_TEST_SERVICE_SHELF_URI,
                                             GG_CAST(self, GG_CoapRequestHandler));

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//! The remote API handler to start/stop the service
GG_Result GG_CoapTestService_HandleRequest(GG_RemoteSmoHandler*  _self,
                                           const char*           request_method,
                                           Fb_Smo*               request_params,
                                           GG_JsonRpcErrorCode*  rpc_error_code,
                                           Fb_Smo**              rpc_result)
{
    GG_COMPILER_UNUSED(request_params);
    GG_COMPILER_UNUSED(rpc_error_code);
    GG_COMPILER_UNUSED(rpc_result);

    GG_CoapTestService* self = GG_SELF(GG_CoapTestService, GG_RemoteSmoHandler);

    if (!strcmp(request_method, GG_RAPI_COAP_TEST_SERVICE_START_METHOD)) {
        // Register coap test service handlers
        GG_Result result = GG_CoapTestService_Register(self);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("GG_CoapTestService handle registration failed (%d)", result);
            return GG_JSON_RPC_ERROR_INTERNAL;
        }
    } else if (!strcmp(request_method, GG_RAPI_COAP_TEST_SERVICE_STOP_METHOD)) {
        // Unregister coap test service handlers
        GG_Result result = GG_CoapTestService_Unregister(self);
        if (GG_FAILED(result)) {
          GG_LOG_WARNING("GG_CoapTestService handle unregistration failed (%d)", result);
          return GG_JSON_RPC_ERROR_INTERNAL;
        }
    } else {
      GG_LOG_WARNING("Unknown remote API method.");
      return GG_JSON_RPC_ERROR_METHOD_NOT_FOUND;
    }

    return GG_SUCCESS;
}

// ------------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapTestService, GG_RemoteSmoHandler) {
    GG_CoapTestService_HandleRequest
};

// ------------------------------------------------------------------------
//! Register the remote shell handlers
GG_Result GG_CoapTestService_RegisterSmoHandlers(GG_RemoteShell*       remote_shell,
                                                 GG_RemoteSmoHandler*  handler)
{
    GG_Result result;
    result = GG_RemoteShell_RegisterSmoHandler(remote_shell,
                                               GG_RAPI_COAP_TEST_SERVICE_START_METHOD,
                                               handler);
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_RemoteShell_RegisterSmoHandler(remote_shell,
                                             GG_RAPI_COAP_TEST_SERVICE_STOP_METHOD,
                                             handler);
}

GG_Result
GG_CoapTestService_UnregisterSmoHandlers(GG_RemoteShell*      remote_shell,
                                         GG_RemoteSmoHandler* handler)
{
    GG_Result result;
    result = GG_RemoteShell_UnregisterSmoHandler(remote_shell,
                                                 GG_RAPI_COAP_TEST_SERVICE_START_METHOD,
                                                 handler);
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_RemoteShell_UnregisterSmoHandler(remote_shell,
                                               GG_RAPI_COAP_TEST_SERVICE_STOP_METHOD,
                                               handler);
}

// ------------------------------------------------------------------------
GG_RemoteSmoHandler* GG_CoapTestService_AsRemoteSmoHandler(GG_CoapTestService* self)
{
    return GG_CAST(self, GG_RemoteSmoHandler);
}

//----------------------------------------------------------------------
//! Called at GG loop init to create coap test service object.
GG_Result GG_CoapTestService_Create(GG_CoapEndpoint*     endpoint,
                                    GG_CoapTestService** service)
{
    if (endpoint == NULL) {
        *service = NULL;
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // Allocate a new object
    GG_CoapTestService* self = (GG_CoapTestService*)GG_AllocateZeroMemory(sizeof(GG_CoapTestService));
    if (self == NULL) {
        *service = NULL;
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // Store the CoAP endpoint of the loop for endpoint registration
    self->endpoint = endpoint;

    // Initialize the shelf list
    GG_LINKED_LIST_INIT(&self->context.shelf_list);

    // setup the interface
    GG_SET_INTERFACE(self, GG_CoapTestService, GG_CoapRequestHandler);
    GG_SET_INTERFACE(self, GG_CoapTestService, GG_RemoteSmoHandler);

    // return the object
    *service = self;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//! Called at GG loop de-init to destroy the coap test service object.
//! Unregisters handlers if registered.
void GG_CoapTestService_Destroy(GG_CoapTestService* self)
{
    if (self == NULL) return;

    GG_CoapTestService_Unregister(self);

    // Free memory
    GG_ClearAndFreeObject(self, 2);
}
