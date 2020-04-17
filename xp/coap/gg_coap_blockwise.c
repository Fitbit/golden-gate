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
 * @date 2018-04-28
 *
 * @details
 *
 * CoAP library - Blockwise transfers
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#define GG_COAP_MESSAGE_PRIVATE
#define GG_COAP_ENDPOINT_PRIVATE

#include "gg_coap.h"
#include "gg_coap_message.h"
#include "gg_coap_endpoint.h"
#include "gg_coap_blockwise.h"
#include "xp/annotations/gg_annotations.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_port.h"
#include "xp/sockets/gg_sockets.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.coap.blockwise")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Object used to keep track of the context associated with a blockwise transfer.
 */
typedef struct {
    GG_IMPLEMENTS(GG_CoapResponseListener);
    GG_IMPLEMENTS(GG_BufferSource);

    GG_LinkedListNode                 list_node;             ///< List node to allow linking this object
    GG_CoapEndpoint*                  endpoint;              ///< Endpoint to which the object belongs
    GG_CoapRequestHandle              handle;                ///< Handle used to identify the request when cancelling
    GG_CoapMethod                     method;                ///< Request method
    GG_CoapBlockwiseResponseListener* listener;              ///< Listener for block/error
    GG_CoapBlockSource*               payload_source;        ///< Source of the request payload, or NULL
    uint32_t                          state;                 ///< Current blockwise transfer state
    size_t                            preferred_block_size;  ///< Preferred block size
    GG_CoapMessageBlockInfo           block2_info;           ///< Values for the BLOCK2 option
    GG_CoapMessageBlockInfo           block1_info;           ///< Values for the BLOCK1 option
    size_t                            block1_payload_size;   ///< Size of the BLOCK1 payload
    GG_CoapMessageOptionParam*        option_params;         ///< Request options in 'params' form
    size_t                            option_count;          ///< Number of options
    GG_CoapRequestHandle              pending_request;       ///< Handle for the latest block request sent
    bool                              use_client_parameters; ///< True if this request should use custom parameters
    GG_CoapClientParameters           client_parameters;     ///< Custom client parameters
    uint8_t                           etag[GG_COAP_MESSAGE_MAX_ETAG_OPTION_SIZE]; ///< ETag
    size_t                            etag_size;                                  ///< ETag sixe
    bool*                             destroy_monitor;       ///< Optional monitor to catch this is destroyed
} GG_CoapBlockwiseRequestContext;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE 1 ///< A BLOCK1 transaction is ongoing
#define GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK2_ACTIVE 2 ///< A BLOCK2 transaction is ongoing
#define GG_COAP_BLOCKWISE_REQUEST_STATE_PAUSED        4 ///< The request is paused

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static GG_Result GG_CoapBlockwiseRequestContext_SendBlockwiseRequest(GG_CoapBlockwiseRequestContext* self);

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/
GG_Result
GG_CoapBlockSource_GetDataSize(GG_CoapBlockSource* self,
                               size_t              offset,
                               size_t*             data_size,
                               bool*               more)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->GetDataSize(self, offset, data_size, more);
}

GG_Result
GG_CoapBlockSource_GetData(GG_CoapBlockSource* self,
                           size_t              offset,
                           size_t              data_size,
                           void*               data)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->GetData(self, offset, data_size, data);
}

void
GG_CoapBlockwiseResponseListener_OnResponseBlock(GG_CoapBlockwiseResponseListener* self,
                                                 GG_CoapMessageBlockInfo*          block_info,
                                                 GG_CoapMessage*                   block_message)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnResponseBlock(self, block_info, block_message);
}

void
GG_CoapBlockwiseResponseListener_OnError(GG_CoapBlockwiseResponseListener* self,
                                         GG_Result                         error,
                                         const char*                       message)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnError(self, error, message);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(size_t offset, size_t* block_size, bool* more, size_t payload_size)
{
    if (offset >= payload_size) {
        return GG_ERROR_OUT_OF_RANGE;
    } else if (offset + *block_size >= payload_size) {
        *more = false;
        *block_size = payload_size - offset;
        return GG_SUCCESS;
    } else {
        *more = true;
        return GG_SUCCESS;
    }
}

//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_Destroy(GG_CoapBlockwiseRequestContext* self)
{
    if (self == NULL) return;

    // remove from the list
    GG_LINKED_LIST_NODE_REMOVE(&self->list_node);

    // cancel any pending block request for this transfer
    if (self->pending_request) {
        GG_CoapEndpoint_CancelRequest(self->endpoint, self->pending_request);
    }

    // cleanup parameters
    if (self->option_params) {
        GG_FreeMemory(self->option_params);
    }

    // if someone's monitoring our destruction, let them know we're gone
    if (self->destroy_monitor) {
        *self->destroy_monitor = true;
    }

    // done
    GG_ClearAndFreeObject(self, 2);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapMessage_GetBlockInfo(const GG_CoapMessage*    self,
                            uint32_t                 block_option_number,
                            GG_CoapMessageBlockInfo* block_info,
                            size_t                   default_block_size)
{
    GG_ASSERT(self);
    GG_ASSERT(block_info);
    GG_ASSERT(block_option_number == GG_COAP_MESSAGE_OPTION_BLOCK1 ||
              block_option_number == GG_COAP_MESSAGE_OPTION_BLOCK2);

    // init the info
    *block_info = (GG_CoapMessageBlockInfo) { 0 };

    // get the option
    GG_CoapMessageOption option;
    GG_Result result = GG_CoapMessage_GetOption(self, block_option_number, &option, 0);
    if (GG_FAILED(result)) {
        if (result == GG_ERROR_NO_SUCH_ITEM && default_block_size != 0) {
            // fill in defaults if requested by the caller
            *block_info = (GG_CoapMessageBlockInfo) {
                .offset = 0,
                .size   = default_block_size,
                .more   = false
            };

            return GG_SUCCESS;
        } else {
            return result;
        }
    }

    // check the option that was found
    if (option.type != GG_COAP_MESSAGE_OPTION_TYPE_UINT) {
        return GG_ERROR_INVALID_FORMAT;
    }

    // parse the option
    uint32_t block_size_log = option.value.uint & 7;
    uint32_t block_index    = option.value.uint >> 4;
    if (block_size_log == 7) {
        return GG_ERROR_INVALID_FORMAT;
    }
    block_info->size   = (1 << (4 + block_size_log));
    block_info->offset = block_info->size * block_index;
    block_info->more   = ((option.value.uint >> 3) & 1) != 0;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapMessageBlockInfo_ToOptionValue(const GG_CoapMessageBlockInfo* blockwise_info, uint32_t* block_option_value)
{
    // compute the block size log
    int      block_size_log = 6;
    uint32_t block_size     = 1024;
    while (block_size != blockwise_info->size && block_size_log >= 0) {
        block_size /= 2;
        --block_size_log;
    }
    if (block_size_log < 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // compute the block index
    uint32_t block_index = (uint32_t)(blockwise_info->offset / blockwise_info->size);

    // pack the blockwise option
    *block_option_value = (block_index << 4) | (blockwise_info->more ? 8 : 0) | (uint32_t)block_size_log;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_CreateBlockwiseResponse(GG_CoapEndpoint*               self,
                                        const GG_CoapMessage*          request,
                                        uint8_t                        code,
                                        GG_CoapMessageOptionParam*     options,
                                        size_t                         options_count,
                                        const uint8_t*                 payload,
                                        size_t                         payload_size,
                                        uint32_t                       block_option_number,
                                        const GG_CoapMessageBlockInfo* block_info,
                                        GG_CoapMessage**               response)
{
    GG_ASSERT(self);
    GG_ASSERT(request);
    GG_ASSERT(block_info);
    GG_ASSERT(response);

    // compute the blockwise option value
    uint32_t block_option_value;
    GG_Result result = GG_CoapMessageBlockInfo_ToOptionValue(block_info, &block_option_value);
    if (GG_FAILED(result)) {
        return result;
    }

    // sanity check that the payload is not more than a block
    if (payload_size > block_info->size) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // setup the option param
    GG_CoapMessageOptionParam option_param = {
        .option.number     = block_option_number,
        .option.type       = GG_COAP_MESSAGE_OPTION_TYPE_UINT,
        .option.value.uint = block_option_value,
        .next              = options // chain with the passed-in options
    };

    // create the response message
    return GG_CoapEndpoint_CreateResponse(self,
                                          request,
                                          code,
                                          &option_param,
                                          options_count + 1,
                                          payload,
                                          payload_size,
                                          response);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_CreateBlockwiseResponseFromBlockSource(GG_CoapEndpoint*               self,
                                                       const GG_CoapMessage*          request,
                                                       uint8_t                        code,
                                                       GG_CoapMessageOptionParam*     options,
                                                       size_t                         options_count,
                                                       GG_CoapBlockSource*            payload_source,
                                                       uint32_t                       block_option_number,
                                                       const GG_CoapMessageBlockInfo* block_info,
                                                       GG_CoapMessage**               response)
{
    GG_ASSERT(self);
    GG_ASSERT(request);
    GG_ASSERT(payload_source);
    GG_ASSERT(block_info);
    GG_ASSERT(response);

    GG_Result result;

    // get the block info
    GG_CoapMessageBlockInfo mutable_block_info = *block_info;
    size_t payload_size = mutable_block_info.size;
    result = GG_CoapBlockSource_GetDataSize(payload_source,
                                            mutable_block_info.offset,
                                            &payload_size,
                                            &mutable_block_info.more);
    if (GG_FAILED(result)) {
        return result;
    }

    // compute the blockwise option value
    uint32_t block_option_value;
    result = GG_CoapMessageBlockInfo_ToOptionValue(&mutable_block_info, &block_option_value);
    if (GG_FAILED(result)) {
        return result;
    }

    // setup the option param
    GG_CoapMessageOptionParam option_param = {
        .option.number     = block_option_number,
        .option.type       = GG_COAP_MESSAGE_OPTION_TYPE_UINT,
        .option.value.uint = block_option_value,
        .next              = options // chain with the passed-in options
    };

    // create the response message without specifying the payload yet (only its size)
    result = GG_CoapEndpoint_CreateResponse(self,
                                            request,
                                            code,
                                            &option_param,
                                            options_count + 1,
                                            NULL,
                                            payload_size,
                                            response);
    if (GG_FAILED(result)) {
        return result;
    }

    // now we can fill in the payload
    result = GG_CoapBlockSource_GetData(payload_source,
                                        mutable_block_info.offset,
                                        payload_size,
                                        GG_CoapMessage_UsePayload(*response));
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("failed to get data from block source (%d)", result);
        GG_CoapMessage_Destroy(*response);
        *response = NULL;
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_OnAck(GG_CoapResponseListener* _self)
{
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
// Notify the listener of an error and terminate
//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(GG_CoapBlockwiseRequestContext* self,
                                                       GG_Result                       error,
                                                       const char*                     message)
{
    // notify the listener
    // NOTE: we setup a monitor so that we can detect if the listener has canceled this request,
    // in which case the `self` object here will have been destroyed when the `OnResponseBlock`
    // callback returns
    if (self->listener) {
        // setup a destroy monitor
        bool destroy_monitor = false;
        self->destroy_monitor = &destroy_monitor;

        // invoke the listener
        GG_CoapBlockwiseResponseListener_OnError(self->listener, error, message);

        // check if this context has been destroyed and exit now if it has
        if (destroy_monitor) {
            GG_LOG_FINE("the request has been canceled by the listener, bailing out");
            return;
        }

        // remove the monitor
        self->destroy_monitor = NULL;
    }

    // done with this request
    GG_CoapBlockwiseRequestContext_Destroy(self);
}

//----------------------------------------------------------------------
// Callback invoked when an error occurs with an individual request
//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_OnError(GG_CoapResponseListener* _self, GG_Result error, const char* message)
{
    GG_CoapBlockwiseRequestContext* self = GG_SELF(GG_CoapBlockwiseRequestContext, GG_CoapResponseListener);

    GG_LOG_FINE("blockwise error: %d %s", error, message ? message : "");

    GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, error, message);
}

//----------------------------------------------------------------------
// Deal with 2.31 responses (GG_COAP_MESSAGE_CODE_CONTINUE)
//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_OnContinueResponse(GG_CoapBlockwiseRequestContext* self, GG_CoapMessage* response)
{
    GG_CoapMessageBlockInfo block_info;
    GG_Result result = GG_CoapMessage_GetBlockInfo(response, GG_COAP_MESSAGE_OPTION_BLOCK1, &block_info, 0);

    if (GG_FAILED(result)) {
        GG_LOG_WARNING("missing or invalid BLOCK1 option (%d)", result);
        GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_COAP_INVALID_RESPONSE, NULL);
        return;
    }

    GG_LOG_FINE("block info: block_offset=%u, block_size=%u, more=%s",
                (int)block_info.offset,
                (int)block_info.size,
                block_info.more ? "true" : "false");

    // prepare for the next block, taking into account the fact that the server may have responded
    // with a block size that is different from what we passed in the request.
    self->block1_info.offset += self->block1_payload_size;
    self->block1_info.size    = GG_MIN(self->block1_info.size, block_info.size);
    self->block1_payload_size = self->block1_info.size;

    result = GG_CoapBlockSource_GetDataSize(self->payload_source,
                                            self->block1_info.offset,
                                            &self->block1_payload_size,
                                            &self->block1_info.more);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("Could not get data size (%d)", result);
        GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, result, NULL);
        return;
    }

    // request the next block
    GG_CoapBlockwiseRequestContext_SendBlockwiseRequest(self);
}

//----------------------------------------------------------------------
// Deal with responses other than 2.31 (GG_COAP_MESSAGE_CODE_CONTINUE)
// ("Final" here doesn't mean it is the last response in the transfer, but that the
// response carries the "final" response code. There may be several such responses)
//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_OnResponseWithFinalResponseCode(GG_CoapBlockwiseRequestContext* self,
                                                               GG_CoapMessage*                 response)
{
    if (self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE) {
        // TODO: deal with cases where the sever sends a success response before the BLOCK1 transfer
        // is finished (not likely, but possible).
        // For now, just assume that a success response is only sent when the BLOCK1 transfer is completed
        GG_LOG_FINE("BLOCK1 request phase completed");
        self->state ^= GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE;
    }

    GG_CoapMessageBlockInfo block2_info;
    GG_Result result = GG_CoapMessage_GetBlockInfo(response, GG_COAP_MESSAGE_OPTION_BLOCK2, &block2_info, 0);
    if (GG_FAILED(result)) {
        if (result == GG_ERROR_NO_SUCH_ITEM) {
            // BLOCK2 option not present, treat this as a "last block" response if this is the first (and only) block
            if (self->block2_info.offset == 0) {
                GG_LOG_FINE("non-blockwise response, simulating a block response");

                // synthesize a BLOCK2 option
                block2_info = (GG_CoapMessageBlockInfo) {
                    .offset = 0,
                    .size   = self->block2_info.size, // use the client-preferred block size
                    .more   = false
                };

                // no BLOCK2 transfer active
                self->state &= ~GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK2_ACTIVE;
            } else {
                // missing BLOCK2 option when one was expected
                GG_LOG_WARNING("missing expected BLOCK2 option (%d)", result);
                GG_LOG_COMMS_ERROR_CODE(GG_LIB_COAP_INVALID_RESPONSE, result);

                GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_COAP_INVALID_RESPONSE, NULL);
                return;
            }
        } else {
            // invalid BLOCK2 option
            GG_LOG_WARNING("invalid BLOCK2 option (%d)", result);
            GG_LOG_COMMS_ERROR_CODE(GG_LIB_COAP_INVALID_RESPONSE, result);

            GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_COAP_INVALID_RESPONSE, NULL);
            return;
        }
    }

    GG_LOG_FINE("block info: block_offset=%u, block_size=%u, more=%s",
                (int)block2_info.offset,
                (int)block2_info.size,
                block2_info.more ? "true" : "false");

    // check that this is the response we expect
    if (block2_info.offset != self->block2_info.offset) {
        GG_LOG_WARNING("received out of sequence block (offset = %u vs %u)",
                       (int)block2_info.offset,
                       (int)self->block2_info.offset);
        GG_LOG_COMMS_ERROR(GG_LIB_COAP_UNEXPECTED_BLOCK);
        GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_COAP_UNEXPECTED_BLOCK, NULL);
        return;
    }

    // notify the listener that we just received a block
    // NOTE: we setup a monitor so that we can detect if the listener has canceled this request,
    // in which case the `self` object here will have been destroyed when the `OnResponseBlock`
    // callback returns
    if (self->listener) {
        // setup a destroy monitor
        bool destroy_monitor = false;
        self->destroy_monitor = &destroy_monitor;

        // invoke the listener
        GG_CoapBlockwiseResponseListener_OnResponseBlock(self->listener, &block2_info, response);

        // check if this context has been destroyed and exit now if it has
        if (destroy_monitor) {
            GG_LOG_FINE("the request has been canceled by the listener, bailing out");
            return;
        }

        // remove the monitor
        self->destroy_monitor = NULL;
    }

    // decide what to do next
    if (block2_info.more) {
        // we have a BLOCK2 phase active
        GG_LOG_FINE("continuing BLOCK2 request phase");
        self->state |= GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK2_ACTIVE;

        // prepare to request the next block
        self->block2_info         = block2_info;
        self->block2_info.offset += self->block2_info.size;
        self->block2_info.more    = false; // The RFC says: in this case the M bit has
                                           // no function and MUST be set to zero.

        // request the next block
        GG_CoapBlockwiseRequestContext_SendBlockwiseRequest(self);
    } else {
        GG_LOG_FINE("BLOCK2 request phase completed");
        self->state &= ~GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK2_ACTIVE;
    }

    // check if we're done
    if (!(self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE) &&
        !(self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK2_ACTIVE)) {
        // done with this request
        GG_LOG_FINE("no more BLOCK transfer active, done with request");
        GG_CoapBlockwiseRequestContext_Destroy(self);
    }
}

//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_OnResponse(GG_CoapResponseListener* _self, GG_CoapMessage* response)
{
    GG_CoapBlockwiseRequestContext* self = GG_SELF(GG_CoapBlockwiseRequestContext, GG_CoapResponseListener);

    GG_LOG_FINE("blockwise response");

    // clear the previous handle
    self->pending_request = 0;

    // check if the response has an ETag
    GG_CoapMessageOption etag_option;
    GG_Result result = GG_CoapMessage_GetOption(response, GG_COAP_MESSAGE_OPTION_ETAG, &etag_option, 0);
    if (GG_SUCCEEDED(result)) {
        GG_LOG_FINER("response has an ETag option");

        // check that the option looks Ok
        if (etag_option.value.opaque.size > GG_COAP_MESSAGE_MAX_ETAG_OPTION_SIZE) {
            GG_LOG_WARNING("invalid ETag option size");
            GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_COAP_INVALID_RESPONSE, NULL);
            return;
        }

        // compare against our ETag field
        if (self->etag_size) {
            if (self->etag_size != etag_option.value.opaque.size ||
                memcmp(self->etag, etag_option.value.opaque.bytes, self->etag_size)) {
                // not the same ETag
                GG_LOG_FINE("ETag mismatch");
                GG_CoapBlockwiseRequestContext_NotifyErrorAndTerminate(self, GG_ERROR_COAP_ETAG_MISMATCH, NULL);
                return;
            }
        } else {
            // remember this ETag
            self->etag_size = etag_option.value.opaque.size;
            memcpy(self->etag, etag_option.value.opaque.bytes, self->etag_size);
        }
    }

    // handle the response as a "continue" or "final" response
    uint8_t code = GG_CoapMessage_GetCode(response);
    if (code == GG_COAP_MESSAGE_CODE_CONTINUE) {
        GG_CoapBlockwiseRequestContext_OnContinueResponse(self, response);
    } else {
        GG_CoapBlockwiseRequestContext_OnResponseWithFinalResponseCode(self, response);
    }

    // NOTE: never access `self` past this point, because the request may have been
    // canceled by now, and `self` would be destroyed as a result.
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapBlockwiseRequestContext, GG_CoapResponseListener) {
    .OnAck      = GG_CoapBlockwiseRequestContext_OnAck,
    .OnError    = GG_CoapBlockwiseRequestContext_OnError,
    .OnResponse = GG_CoapBlockwiseRequestContext_OnResponse
};

//----------------------------------------------------------------------
static size_t
GG_CoapBlockwiseRequestContext_GetDataSize(const GG_BufferSource* _self)
{
    GG_CoapBlockwiseRequestContext* self = GG_SELF(GG_CoapBlockwiseRequestContext, GG_BufferSource);

    return self->block1_payload_size;
}

//----------------------------------------------------------------------
static void
GG_CoapBlockwiseRequestContext_GetData(const GG_BufferSource* _self, void* data)
{
    GG_CoapBlockwiseRequestContext* self = GG_SELF(GG_CoapBlockwiseRequestContext, GG_BufferSource);

    // get the data for the current block
    GG_CoapBlockSource_GetData(self->payload_source,
                               self->block1_info.offset,
                               self->block1_payload_size,
                               data);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapBlockwiseRequestContext, GG_BufferSource) {
    .GetDataSize = GG_CoapBlockwiseRequestContext_GetDataSize,
    .GetData     = GG_CoapBlockwiseRequestContext_GetData
};

//----------------------------------------------------------------------
static GG_Result
GG_CoapBlockwiseRequestContext_SendBlockwiseRequest(GG_CoapBlockwiseRequestContext* self)
{
    GG_CoapMessageOptionParam option_params[3];
    size_t                    option_count = 0;
    uint32_t                  block_option_value;

    // do nothing if the request is paused
    if (self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_PAUSED) {
        GG_LOG_FINE("request is paused, doing nothing now");
        return GG_SUCCESS;
    }

    // init the option params array
    memset(option_params, 0, sizeof(option_params));

    // setup a BLOCK1 option if needed
    if (self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE) {
        GG_CoapMessageBlockInfo_ToOptionValue(&self->block1_info, &block_option_value);
        option_params[option_count].option.number     = GG_COAP_MESSAGE_OPTION_BLOCK1;
        option_params[option_count].option.type       = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option_params[option_count].option.value.uint = block_option_value;
        ++option_count;
    }

    // setup a BLOCK2 option:
    // set the BLOCK2 option if:
    //   * there is an active BLOCK2 transfer, or
    //   * there's no BLOCK1 transfer active, or this is the last BLOCK1 block, and
    //     we have a preferred block size
    if (self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK2_ACTIVE ||
        (self->preferred_block_size &&
         (((self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE) == 0) ||
         ((self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE) && !self->block1_info.more)))) {
        GG_CoapMessageBlockInfo_ToOptionValue(&self->block2_info, &block_option_value);
        option_params[option_count].option.number     = GG_COAP_MESSAGE_OPTION_BLOCK2;
        option_params[option_count].option.type       = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
        option_params[option_count].option.value.uint = block_option_value;
        ++option_count;
    }

    // setup an If-Match option if we have an ETag
    if (self->etag_size) {
        option_params[option_count].option.number             = GG_COAP_MESSAGE_OPTION_IF_MATCH;
        option_params[option_count].option.type               = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE;
        option_params[option_count].option.value.opaque.bytes = self->etag;
        option_params[option_count].option.value.opaque.size  = self->etag_size;
        ++option_count;
    }

    // link the request's client options
    GG_CoapMessageOptionParam* linked_options;
    size_t                     linked_option_count;
    if (option_count) {
        linked_options      = &option_params[0];
        linked_option_count = option_count + self->option_count;
        option_params[option_count - 1].next = self->option_params;
    } else {
        linked_options      = self->option_params;
        linked_option_count = self->option_count;
    }

    // send the request
    GG_Result result;
    if (self->state & GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE) {
        result = GG_CoapEndpoint_SendRequestFromBufferSource(self->endpoint,
                                                             self->method,
                                                             linked_options,
                                                             linked_option_count,
                                                             GG_CAST(self, GG_BufferSource),
                                                             self->use_client_parameters ?
                                                             &self->client_parameters :
                                                             NULL,
                                                             GG_CAST(self, GG_CoapResponseListener),
                                                             &self->pending_request);
    } else {
        result = GG_CoapEndpoint_SendRequest(self->endpoint,
                                             self->method,
                                             linked_options,
                                             linked_option_count,
                                             NULL,
                                             0,
                                             self->use_client_parameters ? &self->client_parameters : NULL,
                                             GG_CAST(self, GG_CoapResponseListener),
                                             &self->pending_request);
    }

    return result;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_SendBlockwiseRequest(GG_CoapEndpoint*                  self,
                                     GG_CoapMethod                     method,
                                     GG_CoapMessageOptionParam*        options,
                                     size_t                            options_count,
                                     GG_CoapBlockSource*               payload_source,
                                     size_t                            preferred_block_size,
                                     const GG_CoapClientParameters*    client_parameters,
                                     GG_CoapBlockwiseResponseListener* listener,
                                     GG_CoapRequestHandle*             request_handle)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // only PUT and POST should have a payload
    GG_ASSERT(!(payload_source && method != GG_COAP_METHOD_PUT && method != GG_COAP_METHOD_POST));

    // try to clone the options
    GG_CoapMessageOptionParam* cloned_options = GG_Coap_CloneOptions(options, options_count);
    if (cloned_options == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // allocate a new context object
    GG_CoapBlockwiseRequestContext* context = GG_AllocateZeroMemory(sizeof(GG_CoapBlockwiseRequestContext));
    if (context == NULL) {
        GG_FreeMemory(cloned_options);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the context
    context->endpoint             = self;
    context->method               = method;
    context->payload_source       = payload_source;
    context->listener             = listener;
    context->preferred_block_size = preferred_block_size;
    context->option_params        = cloned_options;
    context->option_count         = options_count;
    context->handle               = self->blockwise_request_handle_base++;
    if (client_parameters) {
        context->use_client_parameters = true;
        context->client_parameters = *client_parameters;
    }

    // setup interfaces
    GG_SET_INTERFACE(context, GG_CoapBlockwiseRequestContext, GG_CoapResponseListener);
    GG_SET_INTERFACE(context, GG_CoapBlockwiseRequestContext, GG_BufferSource);

    // prepare the initial state
    GG_Result result;
    context->block2_info.size = preferred_block_size ? preferred_block_size : GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE;
    if (method == GG_COAP_METHOD_PUT || method == GG_COAP_METHOD_POST) {
        context->state |= GG_COAP_BLOCKWISE_REQUEST_STATE_BLOCK1_ACTIVE;
        context->block1_info.size = 1024;
        if (payload_source) {
            context->block1_payload_size = context->block1_info.size;
            result = GG_CoapBlockSource_GetDataSize(payload_source,
                                                    context->block1_info.offset,
                                                    &context->block1_payload_size,
                                                    &context->block1_info.more);

            if (GG_FAILED(result)) {
                GG_LOG_WARNING("Could not get data size (%d)", result);
                context->block1_payload_size = 0;
                context->block1_info.more = false;
            }
        }
    }

    // keep track of this request
    GG_LINKED_LIST_APPEND(&self->blockwise_requests, &context->list_node);

    // send the first request
    result = GG_CoapBlockwiseRequestContext_SendBlockwiseRequest(context);
    if (GG_FAILED(result)) {
        GG_CoapBlockwiseRequestContext_Destroy(context);
        return result;
    }

    // return the handle
    if (request_handle) {
        *request_handle = context->handle;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_CancelBlockwiseRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request_handle)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_LINKED_LIST_FOREACH_SAFE(node, &self->blockwise_requests) {
        GG_CoapBlockwiseRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapBlockwiseRequestContext, list_node);
        if (context->handle == request_handle) {
            GG_CoapBlockwiseRequestContext_Destroy(context);
            return GG_SUCCESS;
        }
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
void
GG_CoapEndpoint_DestroyBlockwiseRequestContexts(GG_CoapEndpoint* self)
{
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->blockwise_requests) {
        GG_CoapBlockwiseRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapBlockwiseRequestContext, list_node);
        GG_CoapBlockwiseRequestContext_Destroy(context);
    }
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
void
GG_CoapEndpoint_InspectBlockwiseRequestContexts(GG_CoapEndpoint*            self,
                                                GG_Inspector*               inspector,
                                                const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);

    GG_Inspector_OnArrayStart(inspector, "blockwise_requests");
    GG_LINKED_LIST_FOREACH(node, &self->blockwise_requests) {
        GG_CoapBlockwiseRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapBlockwiseRequestContext, list_node);

        GG_Inspector_OnObjectStart(inspector, NULL);
        GG_Inspector_OnInteger(inspector, "handle", context->handle, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        GG_Inspector_OnInteger(inspector, "method", context->method, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        GG_Inspector_OnInteger(inspector, "state",  context->state,  GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        GG_Inspector_OnInteger(inspector,
                               "preferred_block_size",
                               context->preferred_block_size,
                               GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        GG_Inspector_OnInteger(inspector,
                               "block1_payload_size",
                               context->block1_payload_size,
                               GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        GG_Inspector_OnInteger(inspector,
                               "pending_request",
                               context->pending_request,
                               GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        GG_Inspector_OnObjectEnd(inspector);
    }
    GG_Inspector_OnArrayEnd(inspector);
}
#endif

//----------------------------------------------------------------------
static GG_Result
GG_CoapEndpoint_SetBlockwiseRequestPausedState(GG_CoapEndpoint*     self,
                                               GG_CoapRequestHandle request_handle,
                                               bool                 paused)
{
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->blockwise_requests) {
        GG_CoapBlockwiseRequestContext* context = GG_LINKED_LIST_ITEM(node, GG_CoapBlockwiseRequestContext, list_node);
        if (context->handle == request_handle) {
            if (paused) {
                // pause
                GG_LOG_FINE("pausing request");
                context->state |= GG_COAP_BLOCKWISE_REQUEST_STATE_PAUSED;
            } else if (context->state & GG_COAP_BLOCKWISE_REQUEST_STATE_PAUSED) {
                // resume
                GG_LOG_FINE("resuming request");
                GG_CoapBlockwiseRequestContext_SendBlockwiseRequest(context);
            }
            return GG_SUCCESS;
        }
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_PauseBlockwiseRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request_handle)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return GG_CoapEndpoint_SetBlockwiseRequestPausedState(self, request_handle, true);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapEndpoint_ResumeBlockwiseRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request_handle)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return GG_CoapEndpoint_SetBlockwiseRequestPausedState(self, request_handle, false);
}

//----------------------------------------------------------------------
void
GG_CoapBlockwiseServerHelper_Init(GG_CoapBlockwiseServerHelper* self,
                                  uint32_t                      block_type,
                                  size_t                        preferred_block_size)
{
    // zero-init the structure
    memset(self, 0, sizeof(*self));

    // copy fields or set defaults
    GG_ASSERT(block_type == GG_COAP_MESSAGE_OPTION_BLOCK1 || block_type == GG_COAP_MESSAGE_OPTION_BLOCK2);
    self->block_type = block_type;
    if (preferred_block_size) {
        self->preferred_block_size = preferred_block_size;
    } else {
        self->preferred_block_size = GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE;
    }
}

//----------------------------------------------------------------------
void
GG_CoapBlockwiseServerHelper_SetEtag(GG_CoapBlockwiseServerHelper* self, const uint8_t* etag, size_t etag_size)
{
    // copy the etag
    self->etag_size = GG_MIN(etag_size, GG_COAP_MESSAGE_MAX_ETAG_OPTION_SIZE);
    if (etag_size) {
        GG_ASSERT(etag);
        memcpy(self->etag, etag, self->etag_size);
    }
}

//----------------------------------------------------------------------
GG_Result
GG_CoapBlockwiseServerHelper_OnRequest(GG_CoapBlockwiseServerHelper* self,
                                       const GG_CoapMessage*         request,
                                       bool*                         request_was_resent)
{
    // get the block info from the request
    GG_Result result = GG_CoapMessage_GetBlockInfo(request,
                                                   self->block_type,
                                                   &self->block_info,
                                                   self->preferred_block_size);
    if (GG_FAILED(result)) {
        return GG_COAP_MESSAGE_CODE_BAD_OPTION;
    }

    // check if the request has an If-Match option
    GG_CoapMessageOption if_match_option;
    result = GG_CoapMessage_GetOption(request, GG_COAP_MESSAGE_OPTION_IF_MATCH, &if_match_option, 0);
    if (GG_SUCCEEDED(result)) {
        // check that the option looks Ok
        if (if_match_option.value.opaque.size > GG_COAP_MESSAGE_MAX_ETAG_OPTION_SIZE) {
            return GG_COAP_MESSAGE_CODE_BAD_OPTION;
        }

        // check if there's a match
        if (if_match_option.value.opaque.size != self->etag_size ||
            (self->etag_size && memcmp(if_match_option.value.opaque.bytes, self->etag, self->etag_size))) {
            GG_LOG_WARNING("If-Match Etag value mismatch");
            return GG_COAP_MESSAGE_CODE_PRECONDITION_FAILED;
        }
    }

    // check that the block is either a resent block, or the next expected one
    bool resent = false;
    size_t block_end_offset = self->block_info.offset + GG_CoapMessage_GetPayloadSize(request);
    if (self->block_info.offset == self->next_offset) {
        // this is the next expected block
        if (self->done) {
            // we're done, check that the option is consistent with this state
            if (self->block_info.more) {
                // shouldn't happen
                return GG_COAP_MESSAGE_CODE_BAD_OPTION;
            }
            resent = true;
        }
    } else {
        // this is not the next expected block, check if it is a resent block or a gap
        if (self->block_info.offset) {
            if (block_end_offset != self->next_offset) {
                // gap!
                GG_LOG_WARNING("unexpected block offset (got %u, expected %u)",
                               (int)self->block_info.offset,
                               (int)self->next_offset);
                return GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_INCOMPLETE;
            }
            resent = true;
        } else {
            // new transfer
            self->done = false;
        }
    }

    // update our expectations
    if (!resent) {
        if (self->block_info.more) {
            self->next_offset = block_end_offset;
        } else {
            self->done = true;
        }
    }

    // let the caller know if this was a resent request or not
    if (request_was_resent) {
        *request_was_resent = resent;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapBlockwiseServerHelper_CreateResponse(GG_CoapBlockwiseServerHelper* self,
                                            GG_CoapEndpoint*              endpoint,
                                            const GG_CoapMessage*         request,
                                            uint8_t                       code,
                                            GG_CoapMessageOptionParam*    options,
                                            size_t                        options_count,
                                            const uint8_t*                payload,
                                            size_t                        payload_size,
                                            GG_CoapMessage**              response)
{
    // chain an etag option if we have one
    GG_CoapMessageOptionParam etag_option = GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(ETAG, self->etag, self->etag_size);
    if (self->etag_size) {
        etag_option.next = options;
        options = &etag_option;
        ++options_count;
    }

    return GG_CoapEndpoint_CreateBlockwiseResponse(endpoint,
                                                   request,
                                                   code,
                                                   options,
                                                   options_count,
                                                   payload,
                                                   payload_size,
                                                   self->block_type,
                                                   &self->block_info,
                                                   response);
}
