/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-04-28
 *
 * @details
 *
 * CoAP library - Blockwise Transfers.
 *
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/coap/gg_coap.h"

//! @addtogroup CoAP CoAP
//! CoAP Blockwise Transfers
//! @{

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Details about a block of data.
 */
typedef struct {
    size_t offset; ///< Offset of the block, in bytes, from the start of the resource.
    size_t size;   ///< Size of the block, in bytes.
    bool   more;   ///< `false` if this is the last block of the resource, `true`otherwise.
} GG_CoapMessageBlockInfo;

/**
 * Help object for handling blockwise request.
 * This object is a helper for CoAP server handlers that support BLOCK1 blockwise transfers
 * (i.e PUT or POST requests with large payloads) or BLOCK2 blockwise transfers
 * (i.e GET request with large payloads).
 * This object maintains the current state of a blockwise transfer.
 * This includes the next expected request's block offset,
 * as well as an ETag value used to differentiate between different transfers.
 * Only one transfer can be active at a time.
 * When a request is received, the helper can check if the request is a resent request
 * (the block range is just before the current one) or a new request for the next block.
 *
 * A typical use for this helper is:
 * In the OnRequest() method of a CoAP server handler, when a request is received, call
 * GG_CoapBlockwiseServerHelper_OnRequest() to analyze the request and check that it
 * matches the current expectations. If that method returns an error the handler should
 * terminate and return that error. Otherwise, the handler should check if the requested
 * block is the first block (block 0) of a new transfer (`helper.block_info.offset == 0`).
 * If it is a new transfer, the handler should set the helper's ETag value to differentiate
 * this new transfer from previous ones, by calling GG_CoapBlockwiseServerHelper_SetEtag().
 *
 * NOTE: unfortunately, there's no way to differentiate a new request for block 0 and
 * a retransmitted block 0 request, because for block 0, the ETag value isn't yet known to
 * the client. But that isn't an issue in general, because a retransmitted block 0 would
 * simply start a new session just after the one started by the initial block 0 request.
 *
 * Finally, the handler should:
 *   for BLOCK1 PUT/POST transfers, save/handle/process the block's payload if the block
 *   isn't a resent block,
 *   or
 *   for BLOCK2 GET transfers, prepare the block payload to respond with,
 * then call GG_CoapBlockwiseServerHelper_CreateResponse() to create and return a response.
 * (the call to GG_CoapBlockwiseServerHelper_OnRequest() returns a boolean flag that indicates
 *  whether the request is a new request or a resent request).
 *
 * NOTE: the handler must be prepared to receive the same block request more than
 * once, because transmissions may be lost. The helper object will guarantee that
 * the block requests are never out of order or with gaps, but repeated sequential
 * requests for the same block are possible. For PUT/POST requests, simply ignoring resent
 * blocks is usually sufficient. For GET requests, the handler may want to cache the last
 * returned block if it can't re-generate the block payload.
 */
typedef struct {
    uint32_t                block_type;           ///< GG_COAP_MESSAGE_OPTION_BLOCK1 or GG_COAP_MESSAGE_OPTION_BLOCK2
    size_t                  next_offset;          ///< Next expected block offset
    bool                    done;                 ///< True when we've received the last block
    size_t                  preferred_block_size; ///< Preferred block size
    GG_CoapMessageBlockInfo block_info;           ///< Last parsed BLOCK1 option
    uint8_t                 etag[GG_COAP_MESSAGE_MAX_ETAG_OPTION_SIZE]; ///< ETag for the transfer session
    size_t                  etag_size;                                  ///< ETag size
} GG_CoapBlockwiseServerHelper;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_BLOCKWISE_DEFAULT_BLOCK_SIZE 1024 ///< Default block size

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Get block info from a message's options.
 *
 * @param self The object on which this method is invoked.
 * @param block_option_number The block option to look for
 * (GG_COAP_MESSAGE_OPTION_BLOCK1 or GG_COAP_MESSAGE_OPTION_BLOCK2)
 * @param block_info Pointer to the structure in which the block info will be returned.
 * @param default_block_size Default block size to use if the requested block option isn't found (pass
 * 0 for no default, in which case GG_ERROR_NO_SUCH_ITEM is returned if the option isn't found).
 * The value must be 0, 16, 32, 64, 128, 256, 512, or 1024.
 *
 * @return GG_SUCCESS if the requested block option was found and could be parsed, or a negative error code.
 */
GG_Result GG_CoapMessage_GetBlockInfo(const GG_CoapMessage*    self,
                                      uint32_t                 block_option_number,
                                      GG_CoapMessageBlockInfo* block_info,
                                      size_t                   default_block_size);

/**
 * Given a block info and a total payload size, adjust the block info's `more` field, and
 * return the size of the payload chunk that corresponds to that block of the payload.
 * For all blocks until the last one, this function sets the block's `more` field to `true` and returns the block size.
 * For the last block, this function sets the block's `more` field to `false` and returns a size between
 * 1 and the block size.
 * For blocks that would be outside of the payload, this method just returns 0.
 *
 * @param offset The offset of the block.
 * @param [in,out] size The desired size of the block.
 * @param [out] more Whether there's more data to read.
 * @param payload_size Size of the payload
 *
 * @return GG_SUCCESS if the requested block was not outside of the range.
 */
GG_Result GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(size_t offset, size_t* size, bool* more, size_t payload_size);

/**
 * Interface implemented by objects that are a source of payload blocks for a CoAP blockwise transfer.
 */
GG_DECLARE_INTERFACE(GG_CoapBlockSource) {
    /**
     * Get the data size for a given block.
     *
     * @param self The object on which this method is invoked.
     * @param offset The offset from which to read the data.
     * @param [in,out] data_size The number of bytes to read from that data.
     * @param [out] more Whether there's more data to read.
     *
     * @return GG_SUCCESS if the requested block was not outside of the range.
     */
    GG_Result (*GetDataSize)(GG_CoapBlockSource* self,
                             size_t              offset,
                             size_t*             data_size,
                             bool*               more);

    /**
     * Get the data for a given block.
     *
     * @param self The object on which this method is invoked.
     * @param offset The offset of the requested block in bytes.
     * @param data_size The size of the requested block in bytes.
     * @param data Pointer to the buffer in which the block data should be copied.
     *
     * @return GG_SUCCESS if the data for the requested block could be copied, or a negative error code.
     */
    GG_Result (*GetData)(GG_CoapBlockSource* self,
                         size_t              offset,
                         size_t              data_size,
                         void*               data);
};

//! @relates GG_CoapBlockSource
//! @copydoc GG_CoapBlockSource::GetDataSize
GG_Result GG_CoapBlockSource_GetDataSize(GG_CoapBlockSource* self,
                                         size_t              offset,
                                         size_t*             data_size,
                                         bool*               more);

//! @relates GG_CoapBlockSource
//! @copydoc GG_CoapBlockSource::GetData
GG_Result GG_CoapBlockSource_GetData(GG_CoapBlockSource* self,
                                     size_t              offset,
                                     size_t              data_size,
                                     void*               data);

/**
 * Interface implemented by listeners that want to be notified of CoAP blockwise responses.
 *
 * If `OnError` is called, no other method will be called subsequently.
 */
GG_DECLARE_INTERFACE(GG_CoapBlockwiseResponseListener) {
    /**
     * Method called to notify of the (possibly partial) response.
     *
     * @param self The object on which this method is invoked.
     */
    void (*OnResponseBlock)(GG_CoapBlockwiseResponseListener* self,
                            GG_CoapMessageBlockInfo*          block_info,
                            GG_CoapMessage*                   block_message);

    /**
     * Method called when an error has occurred.
     *
     * @param self The object on which this method is invoked.
     * @param message Error message text (may be NULL).
     */
    void (*OnError)(GG_CoapBlockwiseResponseListener* self, GG_Result error, const char* message);
};

//! @relates GG_CoapBlockwiseResponseListener
//! @copydoc GG_CoapBlockwiseResponseListener::OnResponseBlock
void GG_CoapBlockwiseResponseListener_OnResponseBlock(GG_CoapBlockwiseResponseListener* self,
                                                      GG_CoapMessageBlockInfo*          block_info,
                                                      GG_CoapMessage*                   block_message);

//! @relates GG_CoapBlockwiseResponseListener
//! @copydoc GG_CoapBlockwiseResponseListener::OnError
void GG_CoapBlockwiseResponseListener_OnError(GG_CoapBlockwiseResponseListener* self,
                                              GG_Result error, const char* message);

/**
 * Send a CoAP blockwise request.
 * This method is similar to GG_CoapEndpoint_SendRequest, but with the payload passed as a
 * GG_CoapBlockSource rather than a fixed buffer.
 *
 * @param self The object on which this method is called.
 * @param method Method for the request.
 * @param options Options for the request.
 * @param options_count Number of options for the request.
 * @param payload_source Payload source for the request.
 * @param preferred_block_size Preferred block size. If set to 0, the server's preferred block size
 * will be used.
 * @param client_parameters Optional client parameters to customize the client behavior. Pass NULL for defaults.
 * @param listener Listener object that will receive callbacks regarding any response or error.
 * @param request_handle Handle to the request, that may be used subsequently to cancel the request.
 * (the caller may pass NULL if it isn't interested in the handle value).
 *
 * @return GG_SUCCESS if the request could be sent, or a negative error code.
 */
GG_Result GG_CoapEndpoint_SendBlockwiseRequest(GG_CoapEndpoint*                  self,
                                               GG_CoapMethod                     method,
                                               GG_CoapMessageOptionParam*        options,
                                               size_t                            options_count,
                                               GG_CoapBlockSource*               payload_source,
                                               size_t                            preferred_block_size,
                                               const GG_CoapClientParameters*    client_parameters,
                                               GG_CoapBlockwiseResponseListener* listener,
                                               GG_CoapRequestHandle*             request_handle);

/**
 * Cancel a previously sent blockwise request.
 * When a request is cancelled, its listener will no longer be called, even if a response datagram is
 * received.
 *
 * @param self The object on which this method is called.
 * @parm request_handle Handle of the request to cancel.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_CancelBlockwiseRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request_handle);

/**
 * Pause a blockwise request.
 * When a blockwise request is paused, no further individual block requests will be made until
 * it is resumed.
 * This method may be used by a GG_CoapBlockwiseResponseListener::OnResponseBlock callback if it isn't
 * ready to receive more callbacks (when it is ready again, it can call GG_CoapEndpoint_ResumeBlockwiseRequest).
 *
 * @param self The object on which this method is called.
 * @parm request_handle Handle of the request to pause.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_PauseBlockwiseRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request_handle);

/**
 * Resume a blockwise request.
 * If the request is not paused, this has no effect.
 *
 * @param self The object on which this method is called.
 * @parm request_handle Handle of the request to resume.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapEndpoint_ResumeBlockwiseRequest(GG_CoapEndpoint* self, GG_CoapRequestHandle request_handle);

/**
 * Create a CoAP blockwise response.
 * This method is similar to GG_CoapEndpoint_CreateResponse, but for blockwise responses.
 *
 * @param self The object on which this method is called.
 * @param request The request for which the response is created.
 * @param code The response code.
 * @param options Options for the response.
 * @param options_count Number of options for the response.
 * @param payload Payload for the response.
 * @param payload_size Size of the payload.
 * @param block_option_number Block option number for the response
 * (GG_COAP_MESSAGE_OPTION_BLOCK1 or GG_COAP_MESSAGE_OPTION_BLOCK2)
 * @param block_info Details about the block.
 * @param response Pointer to the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapEndpoint_CreateBlockwiseResponse(GG_CoapEndpoint*               self,
                                                  const GG_CoapMessage*          request,
                                                  uint8_t                        code,
                                                  GG_CoapMessageOptionParam*     options,
                                                  size_t                         options_count,
                                                  const uint8_t*                 payload,
                                                  size_t                         payload_size,
                                                  uint32_t                       block_option_number,
                                                  const GG_CoapMessageBlockInfo* block_info,
                                                  GG_CoapMessage**               response);

/**
 * Create a CoAP blockwise response.
 * This method is similar to GG_CoapEndpoint_CreateBlockwiseResponse, but with the payload supplied
 * by a GG_CoapBlockSource.
 *
 * @param self The object on which this method is called.
 * @param request The request for which the response is created.
 * @param code The response code.
 * @param options Options for the response.
 * @param options_count Number of options for the response.
 * @param payload_source Payload source for the response.
 * @param block_option_number Block option number for the response
 * (GG_COAP_MESSAGE_OPTION_BLOCK1 or GG_COAP_MESSAGE_OPTION_BLOCK2)
 * @param block_info Details about the block.
 * @param response Pointer to the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_CoapEndpoint_CreateBlockwiseResponseFromBlockSource(GG_CoapEndpoint*               self,
                                                                 const GG_CoapMessage*          request,
                                                                 uint8_t                        code,
                                                                 GG_CoapMessageOptionParam*     options,
                                                                 size_t                         options_count,
                                                                 GG_CoapBlockSource*            payload_source,
                                                                 uint32_t                       block_option_number,
                                                                 const GG_CoapMessageBlockInfo* block_info,
                                                                 GG_CoapMessage**               response);

/**
 * Destroy all pending blockwise request contexts.
 *
 * @param self The object on which this method is called.
 */
void GG_CoapEndpoint_DestroyBlockwiseRequestContexts(GG_CoapEndpoint* self);

/**
 * Inspect all pending blockwise request contexts.
 *
 * @param self The object on which this method is called.
 * @param inspector Inspector to call during the inspection.
 * @param options Inspection options.
 */
void GG_CoapEndpoint_InspectBlockwiseRequestContexts(GG_CoapEndpoint*            self,
                                                     GG_Inspector*               inspector,
                                                     const GG_InspectionOptions* options);

/**
 * Encode block info into a block option value.
 *
 * @param blockwise_info The block info structure.
 * @param [out] block_option_value The block option value.
 *
 * @return GG_SUCCESS if successful, or an error code.
 */
GG_Result GG_CoapMessageBlockInfo_ToOptionValue(const GG_CoapMessageBlockInfo*  blockwise_info,
                                                uint32_t*                       block_option_value);

/**
 * Initialize a GG_CoapBlockwiseServerHelper object.
 *
 * @param self The object on which this method is invoked.
 * @param block_type The type of block transfer this object is helping with.
 * (GG_COAP_MESSAGE_OPTION_BLOCK1 for PUT/POST or GG_COAP_MESSAGE_OPTION_BLOCK2 for GET)
 * @param preferred_block_size The preferred block size for the server. Pass 0 to use a default value.
 */
void GG_CoapBlockwiseServerHelper_Init(GG_CoapBlockwiseServerHelper* self,
                                       uint32_t                      block_type,
                                       size_t                        preferred_block_size);

/**
 * Set the ETag value for the current transfer.
 * This is normally called everytime a new transfer starts (i.e a new block with index 0 is received).
 * The Etag value should be as unique as possible, at least within the context of the resource
 * that is exposed by the server.
 *
 * @param etag The ETag value.
 * @param etag_size Size of the ETag value (1 to 8 bytes)
 */
void GG_CoapBlockwiseServerHelper_SetEtag(GG_CoapBlockwiseServerHelper* self, const uint8_t* etag, size_t etag_size);

/**
 * Update the state of the helper when a request is received.
 *
 * @param self The object on which this method is invoked.
 * @param request The request that was received by the server.
 * @param request_was_resent Pointer to where a boolean flag will be returned, indicating whether
 * the request appears to be a resent request or a new request.
 *
 * @return GG_SUCCESS if the request is acceptable, or a non-zero value if the server handler
 * should return without further processing the request (in that case, this return value may be
 * returned as the server OnRequest() handler's return value).
 */
GG_Result GG_CoapBlockwiseServerHelper_OnRequest(GG_CoapBlockwiseServerHelper* self,
                                                 const GG_CoapMessage*         request,
                                                 bool*                         request_was_resent);

/**
 * Create a response based on the previously processed request (GG_CoapBlockwiseServerHelper_OnRequest)
 *
 * @param self The object on which this method is invoked.
 * @param endpoint The endpoint to use to create the response object.
 * @param request The request for which the response is.
 * @param code The response code. Should be GG_COAP_MESSAGE_CODE_CONTINUE for BLOCK1 transfers when
 * the block is not the last block, unless an error condition needs to be returned.
 * @param payload Payload for the response (NULL for BLOCK1).
 * @param payload_size Size of the payload.
 * @param options Optional list of response options (may be NULL if there are none).
 * @param options_count Number of options in the options list.
 * @param response Pointer to variable in which the response should be returned.
 *
 * @return GG_SUCCESS if the response could be created, or a negative error code.
 */
GG_Result GG_CoapBlockwiseServerHelper_CreateResponse(GG_CoapBlockwiseServerHelper* self,
                                                      GG_CoapEndpoint*              endpoint,
                                                      const GG_CoapMessage*         request,
                                                      uint8_t                       code,
                                                      GG_CoapMessageOptionParam*    options,
                                                      size_t                        options_count,
                                                      const uint8_t*                payload,
                                                      size_t                        payload_size,
                                                      GG_CoapMessage**              response);

//! @}

#if defined(__cplusplus)
}
#endif
