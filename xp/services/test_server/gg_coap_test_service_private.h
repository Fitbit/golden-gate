/**
 * @file
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief
 * Fitbit GG CoAP test server helpers.
 */

#include <stdint.h>

#include "xp/coap/gg_coap_blockwise.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_types.h"
#include "xp/remote/gg_remote.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_TEST_SERVER_RESPONSE_OPTION_PAYLOAD_SIZE   50000
#define GG_COAP_TEST_SERVER_RESPONSE_OPTION_PAYLOAD_CRC    50001

#define GG_COAP_TEST_SERVER_MAX_PAYLOAD_CHUNK_SIZE         1024
#define GG_COAP_TEST_SERVER_MAX_SHELF_NAME_LENGTH          128

#define GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE                16

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
//! State enumerator for Test server handler
typedef enum GG_CoapTestServiceState {
    TEST_SERVICE_STATE_IDLE = 0,      ///< No request/respone in progress
    TEST_SERVICE_STATE_BLOCK_RX,      ///< Blockwise request is being received
    TEST_SERVICE_STATE_BLOCK_TX,      ///< Blockwise response is being sent
} GG_CoapTestServiceState;

//! Shelf item (resource) type definition as a GG linked list item
typedef struct GG_CoapTestShelfItem {
    uint8_t             name[GG_COAP_TEST_SERVER_MAX_SHELF_NAME_LENGTH];    ///< Name of the shelved resource
    size_t              name_length;                    ///< Length of the name of the shelved resource
    GG_DynamicBuffer*   payload_buffer;                 ///< Shelved data buffer
    GG_LinkedListNode   node;                           ///< GG linked list node
} GG_CoapTestShelfItem;

//! Context of the test handler
typedef struct GG_CoapTestServiceContext {
    GG_CoapTestServiceState    state;                     ///< State of processing request/response
    uint32_t                   received_payload_size;     ///< Cumulative payload size received so far
    uint32_t                   received_payload_crc;      ///< CRC of the payload received so far
    size_t                     payload_size;              ///< Payload size to be sent
    uint8_t                    response_code;             ///< Response code

    GG_CoapMessageOptionParam  response_options[4];       ///< Response options, payload size/CRC and block1/block2
    uint8_t                    response_options_count;    ///< Number of options populated
    uint8_t                    options_buffer[GG_COAP_TEST_SERVER_MAX_SHELF_NAME_LENGTH];
    ///< Buffer for opaque options value
    uint8_t                    options_buffer_offset;     ///< Next available buffer index in options buffer

    GG_LinkedList              shelf_list;                ///< List of shelf items
    GG_CoapTestShelfItem*      active_unnamed_shelf_item; ///< Unnamed shelf item currently being received
} GG_CoapTestServiceContext;

//! Test request handler for incoming requests
struct GG_CoapTestService{
    GG_IMPLEMENTS(GG_CoapRequestHandler);
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    GG_CoapEndpoint*            endpoint;           ///< coap endpoint service is registered on
    GG_CoapTestServiceContext   context;            ///< Context of the service
};

/*----------------------------------------------------------------------
|   methods
+---------------------------------------------------------------------*/

/**
 * Parse the query options and extract "sz=<payload_size" and "rc=<response_code>"
 *
 * @param request CoAP request
 * @param context Test Server Service context
 */
void GG_CoapTestService_ParseQueryOptions(const GG_CoapMessage*       request,
                                          GG_CoapTestServiceContext*  context);

/**
 * Update the payload size and CRC in the context object with incoming request.
 *
 * @param request CoAP request
 * @param context Test Server Service context
 */
void GG_CoapTestService_UpdateReceivedPayloadInfo(const GG_CoapMessage*       request,
                                                  GG_CoapTestServiceContext*  context);

/**
 * Add the payload size and CRC of the received message to service context
 * to be added to response.
 *
 * @param context Test Server Service context
 */
void GG_CoapTestService_PopulatePayloadSizeAndCRCOptions(GG_CoapTestServiceContext* context);

/**
 * Create a CoAP response. Generate a payload, add block and response options as needed.
 *
 * @param context Test Server Service context
 * @param endpoint CoAP endpoint for the response
 * @param request The CoAP request associated with this response
 * @param payload Payload to be included in the response.
 *                If NULL and payload_size > 0 payload will be generated.
 * @param block1_info Block1 option to be included in response or NULL for excluding block1 option
 * @param block2_info Block2 option to be included in response or NULL for excluding block2 option
 * @param [out] response Response created.
 */
GG_Result GG_CoapTestService_CreateResponse(GG_CoapTestServiceContext*  context,
                                            GG_CoapEndpoint*            endpoint,
                                            const GG_CoapMessage*       request,
                                            const uint8_t*              payload,
                                            size_t                      payload_size,
                                            GG_CoapMessageBlockInfo*    block1_info,
                                            GG_CoapMessageBlockInfo*    block2_info,
                                            GG_CoapMessage**            response);

/**
 * Check for block2 option in request. If no block2 option in request, populate default option
 * if default_block_size > 0. Update the chunk size for the payload to be send in blockwise transfer.
 *
 * @param request CoAP request
 * @param block2_info Block2 option to be updated
 * @param default_block_size 0 if block2 is not optional. Default block size otherwise.
 * @param [out] chunk_size The payload chunk size to be sent in current response
 * @param payload_size total payload size to be sent
 */
GG_Result GG_ProcessBlock2Option(const GG_CoapMessage*      request,
                                 GG_CoapMessageBlockInfo*   block2_info,
                                 size_t                     default_block_size,
                                 size_t*                    chunk_size,
                                 size_t                     payload_size);

/**
 * Create a shelf item and put it on the shelf.
 *
 * @param self Coap test server service object
 * @param item_name The name of the shelf item
 * @param item_name_length The length of the shelf item name
 * @param [out] shelf_item The shelf item created
 */
GG_Result GG_CoapTestService_CreateShelfItem(GG_CoapTestService*     self,
                                             const uint8_t*          item_name,
                                             const size_t            item_name_length,
                                             GG_CoapTestShelfItem**  shelf_item);

/**
 * Delete and distroy an item from shelf.
 *
 * @param shelf_item The item on shelf to be deleted
 */
void GG_CoapTestService_DeleteShelfItem(GG_CoapTestShelfItem* shelf_item);

/**
 * Delete and destroy all items on the shelf.
 *
 * @param self Coap test server service object
 */
void GG_CoapTestService_DeleteAllShelfItems(GG_CoapTestService* self);

/**
 * Dump all the items on shelf using GG_LOG
 *
 * @param self Coap test server service object
 */
void GG_CoapTestService_DumpShelfItems(GG_CoapTestService* self);

/**
 * Find an item on shelf with a given name.
 *
 * @param self Coap test server service object
 * @param item_name The name of the shelf item
 * @param item_name_length The length of the shelf item name
 */
GG_CoapTestShelfItem* GG_CoapTestService_FindShelfItemWithName(GG_CoapTestService*   self,
                                                               const uint8_t*        item_name,
                                                               const size_t          item_name_length);

/**
 * Populate a comma separated list of items on shelf.
 * Allocates buffer internally. Caller should free the buffer.
 *
 * @param self Coap test server service object
 * @param [out] item_names Comma separated list of names of the items on shelf
 * @param [out] item_names_length Length of the list of names
 */
GG_Result GG_GetShelfItemNamesCSV(GG_CoapTestService*  self,
                                  uint8_t**            item_names,
                                  size_t*              item_names_length);
/**
 * Create a shelf item with a generated unique name.
 *
 * @param self Coap test server service object
 * @param [out] shelf_item The shelf item created
 */
GG_Result GG_CoapTestService_CreateUnnamedShelfItem(GG_CoapTestService*      self,
                                                    GG_CoapTestShelfItem**   shelf_item);

/**
 * Set the data of a shelf item.
 *
 * @param shelf_item The shelf to set the data for
 * @param request The request for which payload should be shelved.
 */
GG_Result GG_CoapTestService_SetShelfItemData(GG_CoapTestShelfItem*  shelf_item,
                                              const GG_CoapMessage*  request);

/**
 * Update the data of a shelf item at given offset.
 *
 * @param shelf_item The shelf to set the data for
 * @param offset Offset in the buffer to set the data at.
 * @param request The request for which payload should be shelved.
 */
GG_Result GG_CoapTestService_SetShelfItemDataAtOffset(GG_CoapTestShelfItem*  shelf_item,
                                                      size_t                 offset,
                                                      const GG_CoapMessage*  request);

/**
 * Add the URI location option to response options.
 *
 * @param self Coap test server service object
 */
void GG_AddShelfURIResponseOption(GG_CoapTestService* self);
