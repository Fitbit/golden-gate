/**
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-10
 *
 * @details
 *
 * CoAP library implementation - messages
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#define GG_COAP_MESSAGE_PRIVATE

#include "gg_coap.h"
#include "gg_coap_message.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.coap.message")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_MESSAGE_MIN_SIZE 4

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
void
GG_CoapMessage_Destroy(GG_CoapMessage* self)
{
    if (self == NULL) return;

    if (self->buffer) {
        GG_Buffer_Release(self->buffer);
    }
    GG_ClearAndFreeObject(self, 0);
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
GG_Inspectable*
GG_CoapMessage_AsInspectable(GG_CoapMessage* self)
{
    return GG_CAST(self, GG_Inspectable);
}

//----------------------------------------------------------------------
static GG_Result
GG_CoapMessage_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);

    GG_CoapMessage* self = GG_SELF(GG_CoapMessage, GG_Inspectable);

    const char* type_name = "";
    switch (GG_CoapMessage_GetType(self)) {
        case GG_COAP_MESSAGE_TYPE_CON:
            type_name = "CON";
            break;

        case GG_COAP_MESSAGE_TYPE_NON:
            type_name = "NON";
            break;


        case GG_COAP_MESSAGE_TYPE_ACK:
            type_name = "ACK";
            break;

        case GG_COAP_MESSAGE_TYPE_RST:
            type_name = "RST";
            break;
    }
    GG_Inspector_OnString(inspector, "type", type_name);

    uint8_t code = GG_CoapMessage_GetCode(self);
    if (code > 0 && code < 5) {
        const char* method = "";
        switch (GG_CoapMessage_GetCode(self)) {
            case GG_COAP_METHOD_GET:
                method = "GET";
                break;

            case GG_COAP_METHOD_PUT:
                method = "PUT";
                break;

            case GG_COAP_METHOD_POST:
                method = "POST";
                break;

            case GG_COAP_METHOD_DELETE:
                method = "DELETE";
                break;

            default:
                break;
        }
        GG_Inspector_OnString(inspector, "method", method);
    } else {
        char code_string[7];
        snprintf(code_string,
                 sizeof(code_string),
                 "%d.%02d",
                 GG_COAP_MESSAGE_CODE_CLASS(code),
                 GG_COAP_MESSAGE_CODE_DETAIL(code));
        GG_Inspector_OnString(inspector, "code", code_string);
    }

    GG_Inspector_OnInteger(inspector, "payload_size", self->payload_size, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);

    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length = GG_CoapMessage_GetToken(self, token);
    GG_Inspector_OnBytes(inspector, "token", token, token_length);

    GG_Inspector_OnArrayStart(inspector, "options");
    GG_CoapMessageOptionIterator option_iterator;
    GG_CoapMessage_InitOptionIterator(self, GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY, &option_iterator);
    while (option_iterator.option.number) {
        GG_Inspector_OnObjectStart(inspector, NULL);
        GG_Inspector_OnInteger(inspector, "number", option_iterator.option.number, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
        switch (option_iterator.option.type) {
            case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                GG_Inspector_OnInteger(inspector,
                                       "value",
                                       option_iterator.option.value.uint,
                                       GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_STRING: {
                // the option string isn't null-terminated, so make a local copy, with a max of 31 chars
                char string_value[32];
                size_t string_length = GG_MIN(sizeof(string_value) - 1, option_iterator.option.value.string.length);
                memcpy(string_value, option_iterator.option.value.string.chars, string_length);
                string_value[string_length] = '\0';
                GG_Inspector_OnString(inspector, "value", string_value);
                break;
            }

            case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                GG_Inspector_OnBytes(inspector,
                                     "value",
                                     option_iterator.option.value.opaque.bytes,
                                     option_iterator.option.value.opaque.size);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                break;
        }
        GG_Inspector_OnObjectEnd(inspector);

        GG_CoapMessage_StepOptionIterator(self, &option_iterator);
    }
    GG_Inspector_OnArrayEnd(inspector);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_CoapMessage, GG_Inspectable) {
    .Inspect = GG_CoapMessage_Inspect
};
#endif

/*----------------------------------------------------------------------
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |Ver| T |  TKL  |      Code     |          Message ID           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Token (if any, TKL bytes) ...
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Options (if any) ...
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |1 1 1 1 1 1 1 1|    Payload (if any) ...
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
GG_Result
GG_CoapMessage_CreateFromDatagram(GG_Buffer* datagram, GG_CoapMessage** coap_message)
{
    GG_ASSERT(datagram != NULL);
    GG_ASSERT(coap_message != NULL);

    // default return value
    *coap_message = NULL;

    // sanity checks
    const uint8_t* data      = GG_Buffer_GetData(datagram);
    size_t         data_size = GG_Buffer_GetDataSize(datagram);
    if (data_size < GG_COAP_MESSAGE_MIN_SIZE) {
        GG_LOG_WARNING("datagram too short");
        return GG_ERROR_INVALID_FORMAT;
    }

    // parse the version
    unsigned int version = (data[0] >> 6) & 3;
    if (version != 1) {
        GG_LOG_WARNING("unsupported CoAP version %u", version);
        return GG_ERROR_COAP_UNSUPPORTED_VERSION;
    }

    // obtain a new message
    GG_Result result = GG_SUCCESS;
    GG_CoapMessage* message = (GG_CoapMessage*)GG_AllocateZeroMemory(sizeof(GG_CoapMessage));
    if (message == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // retain the payload buffer
    message->buffer = GG_Buffer_Retain(datagram);
    message->data   = data;

    // parse the token length
    unsigned int token_length = data[0] & 0xF;
    if (token_length > GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH ||
        data_size < 4 + token_length) {
        GG_LOG_WARNING("token length too large");
        result = GG_ERROR_INVALID_FORMAT;
        goto end;
    }

    // parse the options
    data      += 4 + token_length;
    data_size -= 4 + token_length;
    while (data_size && data[0] != 0xFF) {
        unsigned int option_delta  = (data[0] >> 4) & 0xF;
        unsigned int option_length = (data[0]     ) & 0xF;

        if (option_delta == 15 || option_length == 15) {
            GG_LOG_WARNING("invalid delta");
            result = GG_ERROR_INVALID_FORMAT;
            goto end;
        }
        ++data;
        --data_size;

        // extended delta
        if (option_delta == 13) {
            if (data_size < 1) {
                GG_LOG_WARNING("not enough data");
                result = GG_ERROR_INVALID_FORMAT;
                goto end;
            }
            ++data;
            --data_size;
        } else if (option_delta == 14) {
            if (data_size < 2) {
                GG_LOG_WARNING("not enough data");
                result = GG_ERROR_INVALID_FORMAT;
                goto end;
            }
            data      += 2;
            data_size -= 2;
        }

        // extended length
        if (option_length == 13) {
            if (data_size < 1) {
                GG_LOG_WARNING("not enough data");
                result = GG_ERROR_INVALID_FORMAT;
                goto end;
            }
            option_length = 13 + data[0];
            ++data;
            --data_size;
        } else if (option_length == 14) {
            if (data_size < 2) {
                GG_LOG_WARNING("not enough data");
                result = GG_ERROR_INVALID_FORMAT;
                goto end;
            }
            option_length = 269 + ((data[0] << 8) | data[1]);
            data      += 2;
            data_size -= 2;
        }

        // check that we have enough for the option value
        if (data_size < option_length) {
            GG_LOG_WARNING("not enough data");
            result = GG_ERROR_INVALID_FORMAT;
            goto end;
        }

        // move to the next option
        data      += option_length;
        data_size -= option_length;
    }

    // if we have a payload, it must be prefixed with a 0xFF marker
    if (data_size) {
        if (data[0] != 0xFF) {
            GG_LOG_WARNING("expected marker");
            result = GG_ERROR_INVALID_FORMAT;
            goto end;
        }

        // the spec says:
        // "The presence of a marker followed by a zero-length payload MUST
        // be processed as a message format error"
        if (data_size == 1) {
            GG_LOG_WARNING("marker followed by a zero-length payload");
            result = GG_ERROR_INVALID_FORMAT;
            goto end;
        }

        // skip the marker
        ++data;
        --data_size;

        // the payload follows
        message->payload_size = data_size;
    }

    // compute the payload offset
    message->payload_offset = GG_Buffer_GetDataSize(datagram) - data_size;

    // setup interfaces
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(message, GG_CoapMessage, GG_Inspectable));

    // bind to the current thread
    GG_THREAD_GUARD_BIND(message);

end:
    if (result == GG_SUCCESS) {
        *coap_message = message;
    } else {
        if (message) {
            GG_CoapMessage_Destroy(message);
        }
    }
    return result;
}

//----------------------------------------------------------------------
static GG_Result
GG_CoapMessage_GetVarInt(uint32_t n, uint8_t* length_nibble, uint16_t* extension, uint8_t* extension_size)
{
    if (n < 13) {
        if (length_nibble) {
            *length_nibble = (uint8_t)n;
        }
        if (extension_size) {
            *extension_size = 0;
        }
    } else if (n < 269) {
        if (length_nibble) {
            *length_nibble = 13;
        }
        if (extension) {
            *extension = (uint16_t)(n-13);
        }
        if (extension_size) {
            *extension_size = 1;
        }
    } else if (n <= 269+0xFFFF) {
        if (length_nibble) {
            *length_nibble = 14;
        }
        if (extension) {
            *extension = (uint16_t)(n-269);
        }
        if (extension_size) {
            *extension_size = 2;
        }
    } else {
        return GG_ERROR_OUT_OF_RANGE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static size_t
GG_CoapMessage_OptionLength(const GG_CoapMessageOption* option)
{
    switch (option->type) {
        case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
            if (option->value.uint == 0) {
                return 0;
            } else if (option->value.uint < 0xFF) {
                return 1;
            } else if (option->value.uint < 0xFFFF) {
                return 2;
            } else if (option->value.uint < 0xFFFFFF) {
                return 3;
            } else {
                return 4;
            }

        case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
            if (option->value.string.length == 0 && option->value.string.chars) {
                return (size_t)strlen(option->value.string.chars);
            } else {
                return option->value.string.length;
            }

        case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
            return option->value.opaque.size;

        case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
            return 0;
    }

    return 0; // shouldn't be needed, but some linters complain...
}

//----------------------------------------------------------------------
// Compute the size required to encode the options. This assumes that the
// options have already been sorted
//----------------------------------------------------------------------
static size_t
GG_CoapMessage_ComputeOptionsSize(GG_CoapMessageOptionParam* options)
{
    size_t   size = 0;
    uint32_t current_number = 0;

    while (options) {
        GG_ASSERT(options->option.number >= current_number); // options must be in order

        // compute how many bytes are needed to encode the delta
        uint32_t  delta = options->option.number - current_number;
        uint8_t   delta_ext_size;
        GG_Result result = GG_CoapMessage_GetVarInt(delta, NULL, NULL, &delta_ext_size);
        if (GG_FAILED(result)) return size; // stop early if something is wrong

        // compute how many bytes are needed to encode the value
        uint32_t length = (uint32_t)GG_CoapMessage_OptionLength(&options->option);
        uint8_t  length_ext_size;
        result = GG_CoapMessage_GetVarInt(length, NULL, NULL, &length_ext_size);
        if (GG_FAILED(result)) return size; // stop early if something is wrong

        // account for the size of this option
        size += 1 + delta_ext_size + length_ext_size + length;

        // move on to the next option in sorted order
        GG_ASSERT(options->sorted_next != options);
        current_number = options->option.number;
        options = options->sorted_next;
    }

    return size;
}

//----------------------------------------------------------------------
// NOTE: this function doesn't do any bounds checking, so the caller must
// ensure that the buffer is large enough, by calling GG_CoapMessage_ComputeOptionsSize
// before calling this.
// This also assumes that the options have already been sorted.
//----------------------------------------------------------------------
static GG_Result
GG_CoapMessage_SerializeOptions(GG_CoapMessageOptionParam* options, uint8_t* buffer)
{
    uint32_t current_number = 0;

    while (options) {
        GG_ASSERT(options->option.number >= current_number); // options must be in order

        // setup the delta
        uint32_t delta = options->option.number - current_number;
        uint8_t  delta_nibble;
        uint16_t delta_ext      = 0;
        uint8_t  delta_ext_size = 0;
        GG_Result result = GG_CoapMessage_GetVarInt(delta, &delta_nibble, &delta_ext, &delta_ext_size);
        if (GG_FAILED(result)) return result;

        // setup the length
        uint32_t length = (uint32_t)GG_CoapMessage_OptionLength(&options->option);
        uint8_t  length_nibble;
        uint16_t length_ext      = 0;
        uint8_t  length_ext_size = 0;
        result = GG_CoapMessage_GetVarInt(length, &length_nibble, &length_ext, &length_ext_size);
        if (GG_FAILED(result)) return result;

        // delta and length nibbles
        *buffer++ = (uint8_t)((delta_nibble << 4) | length_nibble);

        // delta ext
        if (delta_ext_size == 1) {
            *buffer++ = (uint8_t)delta_ext;
        } else if (delta_ext_size == 2) {
            *buffer++ = (uint8_t)(delta_ext >> 8);
            *buffer++ = (uint8_t)(delta_ext     );
        }

        // length ext
        if (length_ext_size == 1) {
            *buffer++ = (uint8_t)length_ext;
        } else if (length_ext_size == 2) {
            *buffer++ = (uint8_t)(length_ext >> 8);
            *buffer++ = (uint8_t)(length_ext     );
        }

        // value
        if (length) {
            const uint8_t* value = NULL;
            uint8_t bytes[4];
            switch (options->option.type) {
                case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                    GG_BytesFromInt32Be(bytes, options->option.value.uint);
                    value = &bytes[4-length];
                    break;

                case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                    value = (const uint8_t*)options->option.value.string.chars;
                    break;

                case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                    value = (const uint8_t*)options->option.value.opaque.bytes;
                    break;

                case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                    break;
            }
            if (value != NULL) {
                memcpy(buffer, value, length);
            }
            buffer += length;
        }

        // move on to the next option in sorted order
        GG_ASSERT(options->sorted_next != options);
        current_number = options->option.number;
        options = options->sorted_next;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapMessage_Create(uint8_t                    code,
                      GG_CoapMessageType         type,
                      GG_CoapMessageOptionParam* options,
                      size_t                     options_count,
                      uint16_t                   message_id,
                      const uint8_t*             token,
                      size_t                     token_length,
                      const uint8_t*             payload,
                      size_t                     payload_size,
                      GG_CoapMessage**           message)
{
    GG_ASSERT(message);

    // default return value
    *message = NULL;

    // check parameters
    if (token_length > GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    if (options_count && options == NULL) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // link and sort the options
    GG_CoapMessageOptionParam* sorted_options = NULL;
    GG_CoapMessageOptionParam* option_param = &options[0];
    for (size_t i = 0; i < options_count; i++) {
        GG_CoapMessageOptionParam** cursor = &sorted_options;
        while (*cursor && (*cursor)->option.number <= option_param->option.number) {
            cursor = &(*cursor)->sorted_next;
        }

        // link
        option_param->sorted_next = *cursor;
        *cursor = option_param;

        // next
        option_param = GG_COAP_MESSAGE_OPTION_PARAM_NEXT(option_param);;
    }

    // compute the space requirements for the options
    size_t options_size = GG_CoapMessage_ComputeOptionsSize(sorted_options);

    // compute the buffer size needed for the message
    size_t buffer_size = 4 + token_length + options_size + (payload_size ? 1 + payload_size : 0);

    // create the buffer
    GG_DynamicBuffer* buffer = NULL;
    GG_Result result = GG_DynamicBuffer_Create(buffer_size, &buffer);
    if (GG_FAILED(result)) {
        return result;
    }
    GG_DynamicBuffer_SetDataSize(buffer, buffer_size);

    // serialize the message fields
    uint8_t* data = GG_DynamicBuffer_UseData(buffer);
    data[0] = (uint8_t)((1 << 6) | (type << 4) | token_length); // Ver | T | TKL
    data[1] = code;
    data[2] = (message_id >> 8) & 0xFF;
    data[3] = (message_id     ) & 0xFF;
    memcpy(&data[4], token, token_length);
    size_t offset = 4+token_length;

    // options
    GG_CoapMessage_SerializeOptions(sorted_options, &data[offset]);
    offset += options_size;

    // payload
    if (payload_size) {
        // marker
        data[offset++] = 0xFF;

        // copy the payload if specified
        if (payload) {
            memcpy(&data[offset], payload, payload_size);
        } else {
            // payload not yet specified, zero-initialize for now
            memset(&data[offset], 0, payload_size);
        }
    }

    // allocate the message object
    GG_CoapMessage* self = (GG_CoapMessage*)GG_AllocateZeroMemory(sizeof(GG_CoapMessage));
    if (self == NULL) {
        GG_DynamicBuffer_Release(buffer);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    self->buffer         = GG_DynamicBuffer_AsBuffer(buffer);
    self->data           = data;
    self->payload_offset = offset;
    self->payload_size   = payload_size;

    // setup interfaces
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(self, GG_CoapMessage, GG_Inspectable));

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *message = self;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapMessage_ToDatagram(const GG_CoapMessage* self, GG_Buffer** datagram)
{
    GG_ASSERT(self);
    GG_ASSERT(datagram);
    GG_ASSERT(self->buffer);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    *datagram = GG_Buffer_Retain(self->buffer);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_CoapMessageType
GG_CoapMessage_GetType(const GG_CoapMessage* self)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return (GG_CoapMessageType)((self->data[0] >> 4) & 3);
}

//----------------------------------------------------------------------
uint8_t
GG_CoapMessage_GetCode(const GG_CoapMessage* self)
{
    GG_ASSERT(self);
    GG_ASSERT(self->data);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return self->data[1];
}

//----------------------------------------------------------------------
size_t
GG_CoapMessage_GetToken(const GG_CoapMessage* self, uint8_t* token)
{
    GG_ASSERT(self);
    GG_ASSERT(token);
    GG_ASSERT(self->data);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // copy the token value
    size_t token_length = GG_MIN(GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH, self->data[0] & 0xF);
    memcpy(token, self->data + 4, token_length);

    // return the token length
    return token_length;
}

//----------------------------------------------------------------------
uint16_t
GG_CoapMessage_GetMessageId(const GG_CoapMessage* self)
{
    GG_ASSERT(self);
    GG_ASSERT(self->data);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return (uint16_t)((self->data[2] << 8) | self->data[3]);
}

//----------------------------------------------------------------------
size_t
GG_CoapMessage_GetPayloadSize(const GG_CoapMessage* self)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return self->payload_size;
}

//----------------------------------------------------------------------
const uint8_t*
GG_CoapMessage_GetPayload(const GG_CoapMessage* self)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return self->payload_size ? (self->data + self->payload_offset) : NULL;
}

//----------------------------------------------------------------------
uint8_t*
GG_CoapMessage_UsePayload(const GG_CoapMessage* self)
{
    GG_ASSERT(self);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    uint8_t* buffer_data = GG_Buffer_UseData(self->buffer);
    return (self->payload_size != 0 &&  buffer_data != NULL) ?
            buffer_data + self->payload_offset :
            NULL;
}

//----------------------------------------------------------------------
GG_Result
GG_CoapMessage_GetOption(const GG_CoapMessage* self,
                         uint32_t              option_number,
                         GG_CoapMessageOption* option,
                         unsigned int          index)
{
    GG_CoapMessageOptionIterator iterator;
    GG_CoapMessage_InitOptionIterator(self, option_number, &iterator);

    for (;;) {
        if (iterator.option.number == GG_COAP_MESSAGE_OPTION_NONE) {
            // reached the end
            break;
        }
        if (index-- == 0) {
            // found
            *option = iterator.option;
            return GG_SUCCESS;
        }

        // next
        GG_CoapMessage_StepOptionIterator(self, &iterator);
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
void
GG_CoapMessage_InitOptionIterator(const GG_CoapMessage*         self,
                                  uint32_t                      filter,
                                  GG_CoapMessageOptionIterator* iterator)
{
    GG_ASSERT(self);
    GG_ASSERT(iterator);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    iterator->option.number = 0;
    iterator->filter = filter;
    size_t token_length = self->data[0] & 0xF;
    iterator->location = &self->data[4+token_length];
    iterator->end      = &self->data[self->payload_offset];

    GG_CoapMessage_StepOptionIterator(self, iterator);
}

//----------------------------------------------------------------------
// NOTE: this function assumes that the format has already been checked
// so it doesn't do any sanity/bounds checks of its own
//----------------------------------------------------------------------
static void
GG_CoapMessage_StepOptionIterator_(const GG_CoapMessage* self, GG_CoapMessageOptionIterator* iterator)
{
    GG_ASSERT(self);
    GG_ASSERT(iterator);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    if (iterator->location >= iterator->end) {
        // end
        iterator->option.number = GG_COAP_MESSAGE_OPTION_NONE;
        iterator->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_EMPTY;
        return;
    }

    const uint8_t* data = iterator->location;
    unsigned int option_delta  = (data[0] >> 4) & 0xF;
    unsigned int option_length = (data[0]     ) & 0xF;

    if (option_delta == 15 || option_length == 15) {
        // end
        iterator->option.number = GG_COAP_MESSAGE_OPTION_NONE;
        iterator->option.type   = GG_COAP_MESSAGE_OPTION_TYPE_EMPTY;
        return;
    }
    ++data;

    // extended delta
    if (option_delta == 13) {
        option_delta = 13 + *data++;
    } else if (option_delta == 14) {
        option_delta = 269 + ((data[0] << 8) | data[1]);
        data += 2;
    }

    // extended length
    if (option_length == 13) {
        option_length = 13 + *data++;
    } else if (option_length == 14) {
        option_length = 269 + ((data[0] << 8) | data[1]);
        data += 2;
    }

    // update the option number
    iterator->option.number += option_delta;

    // setup the option type if we know it, unless we're skipping this option because of the filter
    if (iterator->filter == 0 || iterator->filter == iterator->option.number) {
        switch (iterator->option.number) {
            case GG_COAP_MESSAGE_OPTION_IF_NONE_MATCH:
                // type EMPTY
                iterator->option.type = GG_COAP_MESSAGE_OPTION_TYPE_EMPTY;
                break;

            case GG_COAP_MESSAGE_OPTION_URI_PORT:
            case GG_COAP_MESSAGE_OPTION_CONTENT_FORMAT:
            case GG_COAP_MESSAGE_OPTION_MAX_AGE:
            case GG_COAP_MESSAGE_OPTION_ACCEPT:
            case GG_COAP_MESSAGE_OPTION_SIZE1:
            case GG_COAP_MESSAGE_OPTION_SIZE2:
            case GG_COAP_MESSAGE_OPTION_BLOCK1:
            case GG_COAP_MESSAGE_OPTION_BLOCK2:
            case GG_COAP_MESSAGE_OPTION_START_OFFSET:
                // type UINT
                iterator->option.type = GG_COAP_MESSAGE_OPTION_TYPE_UINT;
                iterator->option.value.uint = 0;
                for (unsigned int i = 0; i < option_length; i++) {
                    iterator->option.value.uint  = (iterator->option.value.uint  << 8) | data[i];
                }
                break;

            case GG_COAP_MESSAGE_OPTION_URI_HOST:
            case GG_COAP_MESSAGE_OPTION_LOCATION_PATH:
            case GG_COAP_MESSAGE_OPTION_URI_PATH:
            case GG_COAP_MESSAGE_OPTION_URI_QUERY:
            case GG_COAP_MESSAGE_OPTION_LOCATION_QUERY:
            case GG_COAP_MESSAGE_OPTION_PROXY_URI:
            case GG_COAP_MESSAGE_OPTION_PROXY_SCHEME:
                // type STRING
                iterator->option.type = GG_COAP_MESSAGE_OPTION_TYPE_STRING;
                iterator->option.value.string.chars  = (const char*)data;
                iterator->option.value.string.length = option_length;
                break;

            default:
                // use type OPAQUE for all other options
                iterator->option.type = GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE;
                iterator->option.value.opaque.bytes = data;
                iterator->option.value.opaque.size  = option_length;
                break;
        }
    }

    // update the location
    iterator->location = data + option_length;
}

//----------------------------------------------------------------------
void
GG_CoapMessage_StepOptionIterator(const GG_CoapMessage* self, GG_CoapMessageOptionIterator* iterator)
{
    GG_ASSERT(self);
    GG_ASSERT(iterator);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    if (iterator->filter == 0) {
        // no filter, just go to the next option
        GG_CoapMessage_StepOptionIterator_(self, iterator);
    } else {
        // iterate until we find an option with the right number, or reach the end
        do {
            GG_CoapMessage_StepOptionIterator_(self, iterator);
        } while (iterator->option.number && iterator->option.number != iterator->filter);
    }
}
