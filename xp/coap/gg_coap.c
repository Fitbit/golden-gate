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
 * @date 2017-09-18
 *
 * @details
 *
 * CoAP library implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_coap.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_utils.h"

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_CoapResponseListener_OnAck(GG_CoapResponseListener* self)
{
    GG_INTERFACE(self)->OnAck(self);
}

//----------------------------------------------------------------------
void
GG_CoapResponseListener_OnError(GG_CoapResponseListener* self, GG_Result error, const char* message)
{
    GG_INTERFACE(self)->OnError(self, error, message);
}

//----------------------------------------------------------------------
void
GG_CoapResponseListener_OnResponse(GG_CoapResponseListener* self, GG_CoapMessage* response)
{
    GG_INTERFACE(self)->OnResponse(self, response);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapRequestHandler_OnRequest(GG_CoapRequestHandler*   self,
                                GG_CoapEndpoint*         endpoint,
                                const GG_CoapMessage*    request,
                                GG_CoapResponder*        responder,
                                const GG_BufferMetadata* transport_metadata,
                                GG_CoapMessage**         response)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->OnRequest(self, endpoint, request, responder, transport_metadata, response);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapRequestFilter_FilterRequest(GG_CoapRequestFilter* self,
                                   GG_CoapEndpoint*      endpoint,
                                   uint32_t              handler_flags,
                                   const GG_CoapMessage* request,
                                   GG_CoapMessage**      response)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->FilterRequest(self, endpoint, handler_flags, request, response);
}

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_EXTENDED_ERROR_NAMESPACE_FIELD_NUMBER 1
#define GG_COAP_EXTENDED_ERROR_CODE_FIELD_NUMBER      2
#define GG_COAP_EXTENDED_ERROR_MESSAGE_FIELD_NUMBER   3

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_Coap_SplitPathOrQuery(const char*                path_or_query,
                         char                       delimiter,
                         GG_CoapMessageOptionParam* option_params,
                         size_t*                    option_params_count,
                         uint32_t                   option_number)
{
    GG_ASSERT(path_or_query);
    GG_ASSERT(option_params_count);

    // skip leading delimiter characters
    while (*path_or_query == delimiter) {
        ++path_or_query;
    }

    // init the parser state
    size_t components_count = 0;
    const char* cursor = path_or_query;

    // loop while there's something left to parse and some space left in the options array, or we're just measuring
    while (*cursor && (option_params == NULL || components_count < *option_params_count)) {
        // scan characters until we find the delimiter or end of string
        while (*cursor && *cursor != delimiter) {
            ++cursor;
        }

        // check for empty components
        if (cursor == path_or_query) {
            return GG_ERROR_INVALID_SYNTAX;
        }

        // add an option param to the array if one was passed
        if (option_params) {
            option_params->option.number              = option_number;
            option_params->option.type                = GG_COAP_MESSAGE_OPTION_TYPE_STRING;
            option_params->option.value.string.chars  = path_or_query;
            option_params->option.value.string.length = cursor - path_or_query;
            option_params->next                       = NULL;
            ++option_params;
        }

        // advance to the start of the next component
        ++components_count;
        if (*cursor == delimiter) {
            ++cursor;
        }
        path_or_query = cursor;
    }

    // check that we've been able to parse everyting
    if (*cursor) {
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // indicate how many options we have parsed
    *option_params_count = components_count;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_CoapMessageOptionParam*
GG_Coap_CloneOptions(const GG_CoapMessageOptionParam* options, size_t option_count)
{
    // sanity check
    if (option_count > GG_COAP_MESSAGE_MAX_OPTION_COUNT) {
        return NULL;
    }

    // first pass, compute the total size needed
    size_t size = option_count * sizeof(GG_CoapMessageOptionParam);
    const GG_CoapMessageOptionParam* option = options;
    for (size_t i = 0; i < option_count; i++) {
        size_t option_size = 0;
        if (option->option.type == GG_COAP_MESSAGE_OPTION_TYPE_STRING) {
            if (option->option.value.string.length) {
                option_size = option->option.value.string.length;
            } else {
                // null-terminated string
                option_size = strlen(option->option.value.string.chars);
            }
        } else if (option->option.type == GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE) {
            option_size = option->option.value.opaque.size;
        }

        // sanity check
        if (option_size > GG_COAP_MESSAGE_MAX_OPTION_SIZE) {
            return NULL;
        }

        // move on to the next option
        size += option_size;
        option = GG_COAP_MESSAGE_OPTION_PARAM_NEXT(option);
    }

    // allocate the required memory
    GG_CoapMessageOptionParam* cloned = GG_AllocateZeroMemory(size);
    if (cloned == NULL) {
        return NULL;
    }

    // second pass, copy the values
    option = options;
    GG_CoapMessageOptionParam* cursor = cloned;
    uint8_t* data = (uint8_t*)cloned + (option_count * sizeof(GG_CoapMessageOptionParam)); // start of copied data
    for (size_t i = 0; i < option_count; i++) {
        // copy the option info
        cursor->option = option->option;

        // copy the data if needed
        if (option->option.type == GG_COAP_MESSAGE_OPTION_TYPE_STRING) {
            if (option->option.value.string.length) {
                size = option->option.value.string.length;
            } else {
                // null-terminated string
                size = strlen(option->option.value.string.chars);
            }
            memcpy(data, option->option.value.string.chars, size);
            cursor->option.value.string.chars  = (const char*)data;
            cursor->option.value.string.length = size;
            data += size;
        } else if (option->option.type == GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE) {
            size = option->option.value.opaque.size;
            memcpy(data, option->option.value.string.chars, size);
            cursor->option.value.opaque.bytes = (const void*)data;
            cursor->option.value.opaque.size  = size;
            data += size;
        }

        // move on to the next option
        ++cursor;
        option = GG_COAP_MESSAGE_OPTION_PARAM_NEXT(option);
    }

    return cloned;
}


//----------------------------------------------------------------------
size_t
GG_CoapExtendedError_GetEncodedSize(const GG_CoapExtendedError* self)
{
    size_t size = 1 + GG_ProtobufVarintSize(GG_ProtobufSignedToZigZag(self->code));
    if (self->name_space) {
        size_t name_space_size = self->name_space_size ? self->name_space_size : strlen(self->name_space);
        size += 1 + GG_ProtobufVarintSize(name_space_size) + name_space_size;
    }
    if (self->message) {
        size_t message_size = self->message_size ? self->message_size : strlen(self->message);
        size += 1 + GG_ProtobufVarintSize(message_size) + message_size;
    }

    return size;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapExtendedError_Encode(const GG_CoapExtendedError* self,
                            uint8_t*                    buffer)
{
    // encode the namespace: field number = 1, wire type = length-delimited
    if (self->name_space) {
        *buffer++ = GG_PROTOBUF_FIELD_KEY(1, LENGTH_DELIMITED);
        size_t name_space_size = self->name_space_size ? self->name_space_size : strlen(self->name_space);
        buffer += GG_EncodeProtobufVarint(name_space_size, buffer);
        memcpy(buffer, self->name_space, name_space_size);
        buffer += name_space_size;
    }

    // encode the code: field number = 2, wire type = varint (0)
    *buffer++ = GG_PROTOBUF_FIELD_KEY(2, VARINT);;
    buffer += GG_EncodeProtobufVarint(GG_ProtobufSignedToZigZag(self->code), buffer);

    // encode the message: field number = 3, wire type = length-delimited
    if (self->message) {
        *buffer++ = GG_PROTOBUF_FIELD_KEY(3, LENGTH_DELIMITED);;
        size_t message_size = self->message_size ? self->message_size : strlen(self->message);
        buffer += GG_EncodeProtobufVarint(message_size, buffer);
        memcpy(buffer, self->message, message_size);
        buffer += message_size;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapExtendedError_Decode(GG_CoapExtendedError* self,
                            const uint8_t*        payload,
                            size_t                payload_size)
{
    GG_ASSERT(payload);

    // setup default values
    self->name_space      = NULL;
    self->name_space_size = 0;
    self->message         = NULL;
    self->message_size    = 0;
    self->code            = 0;

    // look for the fields we're interested in
    while (payload_size >= 2) {
        unsigned int field_number = (*payload) >> 3;
        unsigned int wire_type    = (*payload) & 7;
        --payload_size;
        ++payload;

        switch (field_number) {
            case GG_COAP_EXTENDED_ERROR_NAMESPACE_FIELD_NUMBER:
            case GG_COAP_EXTENDED_ERROR_MESSAGE_FIELD_NUMBER: {
                // check that this is a string
                if (wire_type != 2) {
                    return GG_ERROR_INVALID_SYNTAX;
                }

                // decode the string
                uint64_t string_size = 0;
                size_t bytes_consumed = GG_DecodeProtobufVarint(payload, payload_size, &string_size);
                if (!bytes_consumed) {
                    return GG_ERROR_INVALID_FORMAT;
                }
                GG_ASSERT(bytes_consumed <= payload_size);
                payload      += bytes_consumed;
                payload_size -= bytes_consumed;

                // check the we have enough data for the string value
                if (payload_size < string_size) {
                    return GG_ERROR_INVALID_FORMAT;
                }

                // store a pointer to the string
                if (field_number == GG_COAP_EXTENDED_ERROR_NAMESPACE_FIELD_NUMBER) {
                    self->name_space      = (const char*)payload;
                    self->name_space_size = (size_t)string_size;
                } else {
                    self->message      = (const char*)payload;
                    self->message_size = (size_t)string_size;
                }
                payload_size -= string_size;
                payload      += string_size;
                break;
            }

            case GG_COAP_EXTENDED_ERROR_CODE_FIELD_NUMBER: {
                // check that this is a varint
                if (wire_type != 0) {
                    return GG_ERROR_INVALID_SYNTAX;
                }

                // decode a zigzag-encoded varint
                uint64_t zigzag_value = 0;
                size_t bytes_consumed = GG_DecodeProtobufVarint(payload, payload_size, &zigzag_value);
                if (!bytes_consumed) {
                    return GG_ERROR_INVALID_FORMAT;
                }

                // convert zigzag to signed interger
                self->code = (int32_t)GG_ProtobufSignedFromZigZag(zigzag_value);

                GG_ASSERT(bytes_consumed <= payload_size);
                payload      += bytes_consumed;
                payload_size -= bytes_consumed;

                break;
            }

            default: {
                size_t field_size = 0;

                // ignore and skip
                switch (wire_type) {
                    case GG_PROTOBUF_WIRE_TYPE_VARINT:
                        field_size = GG_DecodeProtobufVarint(payload, payload_size, NULL);
                        if (field_size == 0) {
                            return GG_ERROR_INVALID_FORMAT;
                        }
                        break;

                    case GG_PROTOBUF_WIRE_TYPE_LENGTH_DELIMITED: {
                        uint64_t length = 0;
                        field_size = GG_DecodeProtobufVarint(payload, payload_size, &length);
                        if (!field_size) {
                            return GG_ERROR_INVALID_FORMAT;
                        }
                        field_size += (size_t)length;
                        break;
                    }

                    case GG_PROTOBUF_WIRE_TYPE_32_BIT:
                        field_size = 4;
                        break;

                    case GG_PROTOBUF_WIRE_TYPE_64_BIT:
                        field_size = 8;
                        break;

                    default:
                        return GG_ERROR_INVALID_FORMAT;
                }
                if (field_size > payload_size) {
                    return GG_ERROR_INVALID_FORMAT;
                }
                payload      += field_size;
                payload_size -= field_size;

                break;
            }
        }
    }

    // check that we have consumed everything
    if (payload_size) {
        return GG_ERROR_INVALID_FORMAT;
    }

    return GG_SUCCESS;
}
