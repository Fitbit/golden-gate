/**
 * @file
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief
 * Fitbit GG CoAP test server helper functions implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <stdlib.h> /* atoi */

#include "xp/common/gg_memory.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_blockwise.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_crc32.h"
#include "xp/common/gg_utils.h"
#include "xp/loop/gg_loop.h"

#include "gg_coap_test_service.h"
#include "gg_coap_test_service_private.h"

GG_SET_LOCAL_LOGGER("gg.xp.services.testserver-private")

/*----------------------------------------------------------------------
|   methods
+---------------------------------------------------------------------*/
//! Generate a payload. That is byte offset (hex) in payload in 4 byte steps.
static void GG_CoapTestService_GeneratePayload(uint8_t* const   payload,
                                               size_t           payload_size,
                                               size_t           offset)
{
    size_t i, j;
    char* pattern = {"0123456789abcdef"};
    offset &= 0xffff;  // keep only 16 bits

    for (i = 0; i < payload_size; i+=4) {
        for (j = 0; j < 4; j++) {
            payload[i+j] = pattern[(i+offset+4)>>((3-j)*4) & 0xf];
        }
    }
}

//----------------------------------------------------------------------
//! Parse the query options and extract requested payload size and
//! requested response code.
//! Parse through the options for queries (filter for queries)
//! All GET/PUT/POST/DELETE can request for payload and response code
//! sz=<payload_size>
//! rc=<response_code>
void GG_CoapTestService_ParseQueryOptions(const GG_CoapMessage*       request,
                                          GG_CoapTestServiceContext*  context)
{
    int response_code = 0;
    GG_CoapMessageOptionIterator options;
    GG_CoapMessage_InitOptionIterator(request, GG_COAP_MESSAGE_OPTION_URI_QUERY, &options);
    while (options.option.number) {
        if (strncmp("sz=", options.option.value.string.chars, 3) == 0) {
            context->payload_size = (size_t)atoi(options.option.value.string.chars+3);
            GG_LOG_FINE("query: payload size: %zu", context->payload_size);
        }
        else if (strncmp("rc=", options.option.value.string.chars, 3) == 0) {
            response_code = atoi(options.option.value.string.chars+3);
            GG_LOG_FINE("query: response code: %d", response_code);
        }
        GG_CoapMessage_StepOptionIterator(request, &options);
    }

    // Validate the response code requested.
    // If nothing requested or 2.31 requested, set according to request method.
    if (response_code == 0 ||
        response_code == GG_COAP_MESSAGE_CODE_CONTINUE) {
        uint8_t  method = GG_CoapMessage_GetCode(request);
        switch (method) {
            case GG_COAP_METHOD_PUT:
            case GG_COAP_METHOD_POST:
                context->response_code = GG_COAP_MESSAGE_CODE_CHANGED;
                break;
            case GG_COAP_METHOD_GET:
                context->response_code = GG_COAP_MESSAGE_CODE_CONTENT;
                break;
            case GG_COAP_METHOD_DELETE:
                context->response_code = GG_COAP_MESSAGE_CODE_DELETED;
                break;
            default:
                context->response_code = GG_COAP_MESSAGE_CODE_BAD_OPTION;
                break;
        }
    } else {
        context->response_code = (uint8_t)GG_COAP_MESSAGE_CODE(response_code);
    }
}

//----------------------------------------------------------------------
//! Update size and CRC32 of the received message payload
void GG_CoapTestService_UpdateReceivedPayloadInfo(const GG_CoapMessage*       request,
                                                  GG_CoapTestServiceContext*  context)
{
    // Get the payload, calculate payload size and CRC-32.
    context->received_payload_size += (uint32_t)GG_CoapMessage_GetPayloadSize(request);

    // Calculate the CRC32
    const uint8_t* payload = GG_CoapMessage_GetPayload(request);
    context->received_payload_crc = GG_Crc32(context->received_payload_crc, payload, context->received_payload_size);

    GG_LOG_INFO("Received payload size: %u CRC: 0x%x", (unsigned int)context->received_payload_size,
                (unsigned int)context->received_payload_crc);
}

//----------------------------------------------------------------------
//! Add the size and CRC of the received payload in response options
void GG_CoapTestService_PopulatePayloadSizeAndCRCOptions(GG_CoapTestServiceContext* context)
{
    // Populate response options
    GG_BytesFromInt32Be(&context->options_buffer[context->options_buffer_offset], context->received_payload_size);
    context->response_options[context->response_options_count] = (GG_CoapMessageOptionParam){
        .option.number              = GG_COAP_TEST_SERVER_RESPONSE_OPTION_PAYLOAD_SIZE,
        .option.type                = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE,
        .option.value.opaque.bytes  = &context->options_buffer[context->options_buffer_offset],
        .option.value.opaque.size   = 4
    };
    ++context->response_options_count;
    context->options_buffer_offset += 4;

    GG_BytesFromInt32Be(&context->options_buffer[context->options_buffer_offset], context->received_payload_crc);
    context->response_options[context->response_options_count] = (GG_CoapMessageOptionParam){
        .option.number              = GG_COAP_TEST_SERVER_RESPONSE_OPTION_PAYLOAD_CRC,
        .option.type                = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE,
        .option.value.opaque.bytes  = &context->options_buffer[context->options_buffer_offset],
        .option.value.opaque.size   = 4
    };
    ++context->response_options_count;
    context->options_buffer_offset += 4;
}

//----------------------------------------------------------------------
//! Add block option to response options
static GG_Result GG_CoapTestService_AddBlockOption(GG_CoapTestServiceContext* context,
                                                   GG_CoapMessageBlockInfo*   block_info,
                                                   uint32_t                   block_option_number)
{
    // compute the blockwise option value
    uint32_t block_option_value;
    GG_Result result = GG_CoapMessageBlockInfo_ToOptionValue(block_info, &block_option_value);
    if (GG_FAILED(result)) {
        return result;
    }

    // add block1 option to response options
    context->response_options[context->response_options_count] = (GG_CoapMessageOptionParam){
        .option.number     = block_option_number,
        .option.type       = GG_COAP_MESSAGE_OPTION_TYPE_UINT,
        .option.value.uint = block_option_value,
        .next              = NULL
    };
    ++context->response_options_count;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//! Create a response with block1/block2 info and generate payload
GG_Result GG_CoapTestService_CreateResponse(GG_CoapTestServiceContext*  context,
                                            GG_CoapEndpoint*            endpoint,
                                            const GG_CoapMessage*       request,
                                            const uint8_t*              payload,
                                            size_t                      payload_size,
                                            GG_CoapMessageBlockInfo*    block1_info,
                                            GG_CoapMessageBlockInfo*    block2_info,
                                            GG_CoapMessage**            response)
{
    uint8_t* _payload = NULL;
    uint8_t response_payload[GG_COAP_TEST_SERVER_MAX_PAYLOAD_CHUNK_SIZE];

    uint8_t code = context->response_code;
    if (block1_info) {
        GG_CoapTestService_AddBlockOption(context, block1_info, GG_COAP_MESSAGE_OPTION_BLOCK1);
        if (block1_info->more) {
            code = GG_COAP_MESSAGE_CODE_CONTINUE;
        }
    }

    if (block2_info) {
        GG_CoapTestService_AddBlockOption(context, block2_info, GG_COAP_MESSAGE_OPTION_BLOCK2);
        if (block2_info->more) {
            code = GG_COAP_MESSAGE_CODE_CONTENT;
        }
        if (payload == NULL) {
            GG_CoapTestService_GeneratePayload(response_payload, payload_size, block2_info->offset);
            _payload = response_payload;
        }
    }

    if (payload == NULL && _payload == NULL && payload_size) {
        // Generate payload of chunk size
        GG_CoapTestService_GeneratePayload(response_payload, payload_size, 0);
        _payload = response_payload;
    }

    return GG_CoapEndpoint_CreateResponse(endpoint,
                                          request,
                                          code,
                                          context->response_options_count ? context->response_options : NULL,
                                          context->response_options_count,
                                          (payload)?payload:_payload, payload_size,
                                          response);
}

//----------------------------------------------------------------------
//! Check if block2 option exists and calculate chunk size.
GG_Result GG_ProcessBlock2Option(const GG_CoapMessage*      request,
                                 GG_CoapMessageBlockInfo*   block2_info,
                                 size_t                     default_block_size,
                                 size_t*                    chunk_size,
                                 size_t                     payload_size)

{
    GG_Result result;
    result = GG_CoapMessage_GetBlockInfo(request, GG_COAP_MESSAGE_OPTION_BLOCK2,
                                         block2_info, default_block_size);
    if (GG_FAILED(result)) {
        return GG_COAP_MESSAGE_CODE_BAD_OPTION;
    }

    GG_LOG_FINE("block2 %d@%d, more=%s",
                (int)block2_info->offset,
                (int)block2_info->size,
                block2_info->more ? "true" : "false");


    // nothing else to do if there's no payload
    if (!payload_size) {
        *chunk_size = 0;
        return GG_SUCCESS;
    }

    // compute the block information
    *chunk_size = block2_info->size;
    result = GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(block2_info->offset,
                                                           chunk_size,
                                                           &block2_info->more,
                                                           payload_size);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("Block info adjustment failed.");
        return GG_COAP_MESSAGE_CODE_INTERNAL_SERVER_ERROR;
    }
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//! Create and put a new item (resource) on the shelf
GG_Result GG_CoapTestService_CreateShelfItem(GG_CoapTestService*     self,
                                             const uint8_t*          item_name,
                                             const size_t            item_name_length,
                                             GG_CoapTestShelfItem**  shelf_item)
{
    GG_Result result;
    *shelf_item = NULL;

    if (item_name_length > GG_COAP_TEST_SERVER_MAX_SHELF_NAME_LENGTH) {
        GG_LOG_WARNING("Shelf item name too long.");
        return GG_COAP_MESSAGE_CODE_BAD_REQUEST;
    }

    // Create the shelf item
    GG_CoapTestShelfItem* _shelf_item;
    _shelf_item = (GG_CoapTestShelfItem*)GG_AllocateZeroMemory(sizeof(GG_CoapTestShelfItem));
    if (!_shelf_item) {
        return GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_TOO_LARGE;
    }

    result = GG_DynamicBuffer_Create(0, &_shelf_item->payload_buffer);
    if (GG_FAILED(result)) {
        return GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_TOO_LARGE;
    }
    GG_LINKED_LIST_NODE_INIT(&_shelf_item->node);
    memcpy(_shelf_item->name, item_name, item_name_length);
    _shelf_item->name_length = item_name_length;

    GG_LINKED_LIST_APPEND(&self->context.shelf_list, &_shelf_item->node);

    // return the shelf item
    *shelf_item = _shelf_item;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//! Remove an item from shelf and distroy it.
void GG_CoapTestService_DeleteShelfItem(GG_CoapTestShelfItem* shelf_item)
{
    GG_DynamicBuffer_Release(shelf_item->payload_buffer);
    GG_LINKED_LIST_NODE_REMOVE(&shelf_item->node);
    GG_FreeMemory(shelf_item);
}

//----------------------------------------------------------------------
//! Remove all items from shelf and distroy them.
void GG_CoapTestService_DeleteAllShelfItems(GG_CoapTestService* self)
{
    GG_CoapTestShelfItem* shelf;
    GG_LINKED_LIST_FOREACH_SAFE(p, &self->context.shelf_list) {
        shelf = GG_LINKED_LIST_ITEM(p, GG_CoapTestShelfItem, node);
        GG_CoapTestService_DeleteShelfItem(shelf);
    }
}

//----------------------------------------------------------------------
//! Print all the shelf items
void GG_CoapTestService_DumpShelfItems(GG_CoapTestService* self)
{
#if defined(GG_CONFIG_ENABLE_LOGGING)
    GG_CoapTestShelfItem* item;
    GG_LOG_FINE("=== List of shelved items:");
    GG_LINKED_LIST_FOREACH(p, &self->context.shelf_list) {
        item = GG_LINKED_LIST_ITEM(p, GG_CoapTestShelfItem, node);
        GG_LOG_FINE("  name: %.*s", (int)item->name_length, item->name);

        size_t         payload_size = GG_DynamicBuffer_GetDataSize(item->payload_buffer);
        const uint8_t* payload      = GG_DynamicBuffer_GetData(item->payload_buffer);

        GG_LOG_FINE("  payload (%zu bytes):", payload_size);
        if (payload_size > 0) {
            for (unsigned int i = 0;
                 i < (payload_size + GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE - 1)/ GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE;
                 i++) {
                char hex_buffer[(2 * GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE) + 1];
                char str_buffer[GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE + 1];
                memset(hex_buffer, ' ', sizeof(hex_buffer) - 1);
                memset(str_buffer, ' ', sizeof(str_buffer) - 1);
                size_t chunk = GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE;
                if (i * GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE + chunk > payload_size) {
                    chunk = payload_size - (i * GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE);
                }
                GG_BytesToHex(&payload[i * GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE], chunk, hex_buffer, true);
                for (unsigned int j = 0; j < chunk; j++) {
                    uint8_t c = payload[i * GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE + j];
                    if (c >= 0x20 && c <= 0x7E) {
                        str_buffer[j] = (char)c;
                    } else {
                        str_buffer[j] = '.';
                    }
                }
                str_buffer[sizeof(str_buffer) - 1] = '\0';
                hex_buffer[sizeof(hex_buffer) - 1] = '\0';
                GG_LOG_FINE("  %04d: %s    %s", i * GG_COAP_TEST_SERVER_DUMP_CHUNK_SIZE, str_buffer, hex_buffer);
            }
        }
    }
    GG_LOG_FINE("=== end of shelved items.");
#else
    GG_COMPILER_UNUSED(self);
#endif
}

//----------------------------------------------------------------------
//! Find a shelf from name or return NULL
GG_CoapTestShelfItem* GG_CoapTestService_FindShelfItemWithName(GG_CoapTestService*   self,
                                                               const uint8_t*        item_name,
                                                               const size_t          item_name_length)
{
    GG_CoapTestShelfItem* item;
    GG_LINKED_LIST_FOREACH(p, &self->context.shelf_list) {
        item = GG_LINKED_LIST_ITEM(p, GG_CoapTestShelfItem, node);
        if (item->name_length == item_name_length &&
            memcmp(item_name, item->name, item->name_length) == 0) {
            return item;
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
//! Get a comma separate list of all the shelved items. (Buffer is allocated internally).
GG_Result GG_GetShelfItemNamesCSV(GG_CoapTestService*  self,
                                  uint8_t**            item_names,
                                  size_t*              item_names_length)
{
    // Get the number of shelves and sum of their name lengths
    size_t item_count = 0;
    size_t name_length_sum = 0;

    GG_CoapTestShelfItem* item = NULL;
    GG_LINKED_LIST_FOREACH(p, &self->context.shelf_list) {
        ++item_count;
        item = GG_LINKED_LIST_ITEM(p, GG_CoapTestShelfItem, node);
        name_length_sum += item->name_length;
    }

    if (item_count == 0) {
        *item_names = NULL;
        *item_names_length = 0;
        return GG_SUCCESS;
    }

    // Create a buffer with room for names and commas
    uint8_t* names = GG_AllocateMemory(sizeof(uint8_t)*(item_count+name_length_sum));
    if (names == NULL) {
        *item_names = NULL;
        *item_names_length = 0;
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // Concatenate all shelf item names
    size_t offset = 0;
    GG_LINKED_LIST_FOREACH(p, &self->context.shelf_list) {
        item = GG_LINKED_LIST_ITEM(p, GG_CoapTestShelfItem, node);
        memcpy(names+offset, item->name, item->name_length);
        offset += item->name_length;
        names[offset] = ',';
        ++offset;
    }

    *item_names = names;
    *item_names_length = offset - 1;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//! Get a unique shelf item name
static void GG_CoapTestService_GetUniqueShelfItemName(GG_CoapTestService*   self,
                                                      uint8_t*              item_name,
                                                      size_t*               item_name_length)
{
    int index = 0;
    GG_CoapTestShelfItem* shelf_item;
    do {
        snprintf((char*)item_name, GG_COAP_TEST_SERVER_MAX_SHELF_NAME_LENGTH, "unnamed%d", index);
        *item_name_length = strlen((char*)item_name);
        shelf_item = GG_CoapTestService_FindShelfItemWithName(self, item_name, *item_name_length);

        index++;
    } while (shelf_item != NULL);
}

//----------------------------------------------------------------------
//! Create an unnamed shelf item
GG_Result GG_CoapTestService_CreateUnnamedShelfItem(GG_CoapTestService*      self,
                                                    GG_CoapTestShelfItem**   shelf_item)
{
    uint8_t name[GG_COAP_TEST_SERVER_MAX_SHELF_NAME_LENGTH];
    size_t name_length;
    GG_CoapTestService_GetUniqueShelfItemName(self, name, &name_length);
    return GG_CoapTestService_CreateShelfItem(self, name, name_length, shelf_item);
}

//----------------------------------------------------------------------
//! Set the request payload to the shelf item
GG_Result GG_CoapTestService_SetShelfItemData(GG_CoapTestShelfItem*  shelf_item,
                                              const GG_CoapMessage*  request)
{
    GG_Result result;
    // Get the payload from the message and append it to the shelf item
    size_t payload_size = GG_CoapMessage_GetPayloadSize(request);
    const uint8_t *payload = GG_CoapMessage_GetPayload(request);

    result = GG_DynamicBuffer_SetData(shelf_item->payload_buffer,
                                      payload, payload_size);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("Could not shelf payload");
        return GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_TOO_LARGE;
    }
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//! Set the request payload to the shelf item
GG_Result GG_CoapTestService_SetShelfItemDataAtOffset(GG_CoapTestShelfItem*  shelf_item,
                                                      size_t                 offset,
                                                      const GG_CoapMessage*  request)
{
    GG_Result result;
    // Get the payload from the message and append it to the shelf item
    size_t payload_size = GG_CoapMessage_GetPayloadSize(request);
    const uint8_t *payload = GG_CoapMessage_GetPayload(request);

    // Check if there is enough buffer space
    size_t data_size = offset + payload_size;
    if (data_size > GG_DynamicBuffer_GetBufferSize(shelf_item->payload_buffer)) {
        // Extend the buffer size.
        result = GG_DynamicBuffer_SetDataSize(shelf_item->payload_buffer, data_size);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("Could not shelf payload");
            return GG_COAP_MESSAGE_CODE_REQUEST_ENTITY_TOO_LARGE;
        }
    }

    uint8_t* payload_buffer = GG_DynamicBuffer_UseData(shelf_item->payload_buffer);
    memcpy(payload_buffer+offset, payload, payload_size);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void GG_AddShelfURIResponseOption(GG_CoapTestService* self)
{
    GG_CoapTestShelfItem* shelf_item = self->context.active_unnamed_shelf_item;
    GG_CoapTestServiceContext* context = &self->context;
    memcpy(&context->options_buffer[context->options_buffer_offset],
           shelf_item->name, shelf_item->name_length);
    context->response_options[context->response_options_count] = (GG_CoapMessageOptionParam) {
        .option.number              = GG_COAP_MESSAGE_OPTION_LOCATION_PATH,
        .option.type                = GG_COAP_MESSAGE_OPTION_TYPE_STRING,
        .option.value.string.chars  = (const char*)&context->options_buffer[context->options_buffer_offset],
        .option.value.string.length = shelf_item->name_length
    };
    context->options_buffer_offset += shelf_item->name_length;
    ++context->response_options_count;
}
