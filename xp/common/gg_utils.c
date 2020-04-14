/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 *
 * @details
 *
 * Implementation of utility functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdbool.h>

#include "gg_utils.h"
#include "gg_port.h"
#include "gg_types.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
static const signed char GG_Base64_Bytes[128] = {
      -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x3E,   -1, 0x3E,   -1, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,   -1,   -1,   -1,   -1,   -1,   -1,
      -1, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,   -1,   -1,   -1,   -1, 0x3F,
      -1, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33,   -1,   -1,   -1,   -1,   -1
};

static const char GG_Base64_Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define GG_BASE64_MAX_ENCODE_INPUT_SIZE (3 * (((size_t)(-1)) / 4))

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/
void
GG_GenericCallbackHandler_OnCallback(GG_GenericCallbackHandler* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnCallback(self);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_SimpleCallback_OnCallback(GG_GenericCallbackHandler* _self)
{
    GG_SimpleCallback* self = GG_SELF(GG_SimpleCallback, GG_GenericCallbackHandler);
    if (self->handler) {
        self->handler(self->state);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_SimpleCallback, GG_GenericCallbackHandler) {
    .OnCallback = GG_SimpleCallback_OnCallback
};

//----------------------------------------------------------------------
GG_GenericCallbackHandler*
GG_SimpleCallback_Init(GG_SimpleCallback* callback, void (*handler)(void* state), void* state)
{
    // init the object fields
    callback->handler = handler;
    callback->state   = state;

    // init the interface
    GG_SET_INTERFACE(callback, GG_SimpleCallback, GG_GenericCallbackHandler);

    // return a pointer to the interface
    return GG_CAST(callback, GG_GenericCallbackHandler);
}

//----------------------------------------------------------------------
GG_Result
GG_Base64_Encode(const uint8_t* input,
                 size_t         input_size,
                 char*          output,
                 size_t*        output_size,
                 bool           url_safe)
{
    GG_ASSERT(input);
    GG_ASSERT(output_size);
    GG_ASSERT(!(output == NULL && *output_size != 0));

    char* output_shadow = output;

    // check that the output buffer is large enough
    if (input_size > GG_BASE64_MAX_ENCODE_INPUT_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    size_t blocks_needed = (input_size + 2)/3;
    if (*output_size / 4 < blocks_needed) {
        *output_size = 4 * blocks_needed;
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }
    *output_size = 4 * blocks_needed;

    // encode each byte
    size_t i = 0;
    while (input_size >= 3) {
        // output a block
        *output++ = GG_Base64_Chars[ (input[i  ] >> 2) & 0x3F];
        *output++ = GG_Base64_Chars[((input[i  ] & 0x03) << 4) | ((input[i+1] >> 4) & 0x0F)];
        *output++ = GG_Base64_Chars[((input[i+1] & 0x0F) << 2) | ((input[i+2] >> 6) & 0x03)];
        *output++ = GG_Base64_Chars[  input[i+2] & 0x3F];

        input_size -= 3;
        i += 3;
    }

    // deal with the tail
    if (input_size == 2) {
        *output++ = GG_Base64_Chars[ (input[i  ] >> 2) & 0x3F];
        *output++ = GG_Base64_Chars[((input[i  ] & 0x03) << 4) | ((input[i+1] >> 4) & 0x0F)];
        *output++ = GG_Base64_Chars[ (input[i+1] & 0x0F) << 2];
        *output++ = '=';
    } else if (input_size == 1) {
        *output++ = GG_Base64_Chars[(input[i] >> 2) & 0x3F];
        *output++ = GG_Base64_Chars[(input[i] & 0x03) << 4];
        *output++ = '=';
        *output++ = '=';
    }

    // deal with url safe remapping
    output = output_shadow;  // Rewind the output pointer
    if (url_safe) {
        for (i = 0; i < *output_size; i++) {
            if (output[i] == '+') {
                output[i] = '-';
            } else if (output[i] == '/') {
                output[i] = '_';
            }
        }
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Base64_Decode(const char* input,
                 size_t      input_size,
                 uint8_t*    output,
                 size_t*     output_size,
                 bool        url_safe)
{
    GG_ASSERT(input);
    GG_ASSERT(output_size);
    GG_ASSERT(!(output == NULL && *output_size != 0));

    // an input size of 0 means that we should measure the string
    if (input_size == 0) {
        input_size = strlen(input);
    }

    // init counters
    size_t       buffer_size = *output_size;
    unsigned int padding = 0;
    size_t       char_count = 0;

    // default return value
    *output_size = 0;

    // first check the input
    for (size_t i = 0; i < input_size; i++) {
        int c = (int)input[i];

        // skip \r and \n
        if (c == '\r' || c == '\n') {
            continue;
        }

        // check for padding characters
        if (c == '=') {
            if (++padding > 2) {
                // too much padding
                return GG_ERROR_INVALID_FORMAT;
            }
            continue;
        } else if (padding) {
            // padding is only allowed at the end
            return GG_ERROR_INVALID_FORMAT;
        } else {
            // check for alternate charsets
            if (url_safe) {
                if (c == '+' || c == '/') {
                    return GG_ERROR_INVALID_FORMAT;
                }
            } else {
                if (c == '-' || c == '_') {
                    return GG_ERROR_INVALID_FORMAT;
                }
            }

            // check for valid characters
            if (c > 127 || GG_Base64_Bytes[c] < 0) {
                return GG_ERROR_INVALID_FORMAT;
            }
        }

        ++char_count;
    }

    // check for invalid tails
    if ((char_count % 4) == 1) {
        return GG_ERROR_INVALID_FORMAT;
    }

    // check that we have enough space for the output
    size_t input_groups = char_count/4;
    *output_size = 3 * input_groups;
    if (char_count % 4) {
        *output_size += (char_count % 4) - 1;
    }
    if (buffer_size < *output_size) {
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // special exit case if the output is empty
    if (*output_size == 0) {
        return GG_SUCCESS;
    }

    // now decode whole blocks
    char_count = 0;
    uint32_t accumulator = 0;
    for (size_t i = 0; i < input_size; i++) {
        int c = (int)input[i];

        // skip \r and \n
        if (c == '\r' || c == '\n') {
            continue;
        }

        // terminate on padding
        if (c == '=') {
            break;
        }

        // get 6 more bits
        accumulator = (accumulator << 6) | GG_Base64_Bytes[c];

        // decode when a block is full
        if ((++char_count % 4) == 0) {
            *output++ = (uint8_t)(accumulator >> 16);
            *output++ = (uint8_t)(accumulator >>  8);
            *output++ = (uint8_t)(accumulator      );
        }
    }

    // decode the tail, if any
    size_t tail = char_count % 4;
    if (tail == 3) {
        *output++ = (uint8_t)(accumulator >> 10);
        *output++ = (uint8_t)(accumulator >>  2);
    } else if (tail >= 2) {
        *output++ = (uint8_t)(accumulator >> 4);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_BytesFromInt16Be(uint8_t* buffer, uint16_t value)
{
    buffer[0] = (uint8_t)((value >> 8) & 0xFF);
    buffer[1] = (uint8_t)((value     ) & 0xFF);
}

//----------------------------------------------------------------------
void
GG_BytesFromInt32Be(uint8_t* buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 24) & 0xFF;
    buffer[1] = (uint8_t)(value >> 16) & 0xFF;
    buffer[2] = (uint8_t)(value >>  8) & 0xFF;
    buffer[3] = (uint8_t)(value      ) & 0xFF;
}

//----------------------------------------------------------------------
void
GG_BytesFromInt64Be(uint8_t* buffer, uint64_t value)
{
    buffer[0] = (uint8_t)(value >> 56) & 0xFF;
    buffer[1] = (uint8_t)(value >> 48) & 0xFF;
    buffer[2] = (uint8_t)(value >> 40) & 0xFF;
    buffer[3] = (uint8_t)(value >> 32) & 0xFF;
    buffer[4] = (uint8_t)(value >> 24) & 0xFF;
    buffer[5] = (uint8_t)(value >> 16) & 0xFF;
    buffer[6] = (uint8_t)(value >>  8) & 0xFF;
    buffer[7] = (uint8_t)(value      ) & 0xFF;
}

//----------------------------------------------------------------------
uint16_t
GG_BytesToInt16Be(const uint8_t* buffer)
{
    return (uint16_t)
        (((uint16_t)buffer[0]) << 8) |
        (((uint16_t)buffer[1])     );
}

//----------------------------------------------------------------------
uint32_t
GG_BytesToInt32Be(const uint8_t* buffer)
{
    return
        (((uint32_t)buffer[0]) << 24) |
        (((uint32_t)buffer[1]) << 16) |
        (((uint32_t)buffer[2]) <<  8) |
        (((uint32_t)buffer[3])      );
}

//----------------------------------------------------------------------
uint64_t
GG_BytesToInt64Be(const uint8_t* buffer)
{
    return
        (((uint64_t)buffer[0]) << 56) |
        (((uint64_t)buffer[1]) << 48) |
        (((uint64_t)buffer[2]) << 40) |
        (((uint64_t)buffer[3]) << 32) |
        (((uint64_t)buffer[4]) << 24) |
        (((uint64_t)buffer[5]) << 16) |
        (((uint64_t)buffer[6]) <<  8) |
        (((uint64_t)buffer[7])      );
}

//----------------------------------------------------------------------
void
GG_BytesFromInt16Le(uint8_t* buffer, uint16_t value)
{
    buffer[1] = (uint8_t)((value >> 8) & 0xFF);
    buffer[0] = (uint8_t)((value     ) & 0xFF);
}

//----------------------------------------------------------------------
void
GG_BytesFromInt32Le(uint8_t* buffer, uint32_t value)
{
    buffer[3] = (uint8_t)(value >> 24) & 0xFF;
    buffer[2] = (uint8_t)(value >> 16) & 0xFF;
    buffer[1] = (uint8_t)(value >>  8) & 0xFF;
    buffer[0] = (uint8_t)(value      ) & 0xFF;
}

//----------------------------------------------------------------------
void
GG_BytesFromInt64Le(uint8_t* buffer, uint64_t value)
{
    buffer[7] = (uint8_t)(value >> 56) & 0xFF;
    buffer[6] = (uint8_t)(value >> 48) & 0xFF;
    buffer[5] = (uint8_t)(value >> 40) & 0xFF;
    buffer[4] = (uint8_t)(value >> 32) & 0xFF;
    buffer[3] = (uint8_t)(value >> 24) & 0xFF;
    buffer[2] = (uint8_t)(value >> 16) & 0xFF;
    buffer[1] = (uint8_t)(value >>  8) & 0xFF;
    buffer[0] = (uint8_t)(value      ) & 0xFF;
}

//----------------------------------------------------------------------
uint16_t
GG_BytesToInt16Le(const uint8_t* buffer)
{
    return (uint16_t)
        (((uint16_t)buffer[1]) << 8) |
        (((uint16_t)buffer[0])     );
}

//----------------------------------------------------------------------
uint32_t
GG_BytesToInt32Le(const uint8_t* buffer)
{
    return
        (((uint32_t)buffer[3]) << 24) |
        (((uint32_t)buffer[2]) << 16) |
        (((uint32_t)buffer[1]) <<  8) |
        (((uint32_t)buffer[0])      );
}

//----------------------------------------------------------------------
uint64_t
GG_BytesToInt64Le(const uint8_t* buffer)
{
    return
        (((uint64_t)buffer[7]) << 56) |
        (((uint64_t)buffer[6]) << 48) |
        (((uint64_t)buffer[5]) << 40) |
        (((uint64_t)buffer[4]) << 32) |
        (((uint64_t)buffer[3]) << 24) |
        (((uint64_t)buffer[2]) << 16) |
        (((uint64_t)buffer[1]) <<  8) |
        (((uint64_t)buffer[0])      );
}

//----------------------------------------------------------------------
char
GG_NibbleToHex(unsigned int nibble, bool uppercase)
{
    if (uppercase) {
        return (char)((nibble < 10) ? ('0' + nibble) : ('A' + (nibble-10)));
    } else {
        return (char)((nibble < 10) ? ('0' + nibble) : ('a' + (nibble-10)));
    }
}

//----------------------------------------------------------------------
void
GG_ByteToHex(uint8_t b, char* buffer, bool uppercase)
{
    buffer[0] = GG_NibbleToHex((b >> 4) & 0x0F, uppercase);
    buffer[1] = GG_NibbleToHex(b        & 0x0F, uppercase);
}

//----------------------------------------------------------------------
void
GG_BytesToHex(const uint8_t* bytes, size_t bytes_size, char* hex, bool uppercase)
{
    for (unsigned int i = 0; i < bytes_size; i++) {
        GG_ByteToHex(bytes[i], hex, uppercase);
        hex += 2;
    }
}

//----------------------------------------------------------------------
void
GG_BytesToHexString(const uint8_t* bytes, size_t bytes_size, GG_String* hex, bool uppercase)
{
    GG_String_Reserve(hex, bytes_size * 2);

    char* s = GG_String_UseChars(hex);
    for (unsigned int i = 0; i < bytes_size; i++) {
        GG_ByteToHex(bytes[i], s, uppercase);
        s += 2;
    }

    GG_String_SetLength(hex, bytes_size * 2);
}

//----------------------------------------------------------------------
int
GG_HexToNibble(char hex)
{
    if (hex >= 'a' && hex <= 'f') {
        return ((hex - 'a') + 10);
    } else if (hex >= 'A' && hex <= 'F') {
        return ((hex - 'A') + 10);
    } else if (hex >= '0' && hex <= '9') {
        return (hex - '0');
    } else {
        return -1;
    }
}

//----------------------------------------------------------------------
GG_Result
GG_HexToByte(const char* buffer, uint8_t* byte)
{
    int nibble_0 = GG_HexToNibble(buffer[0]);
    int nibble_1 = GG_HexToNibble(buffer[1]);

    if (nibble_0 < 0 || nibble_1 < 0) {
        return GG_ERROR_INVALID_SYNTAX;
    }

    *byte = (uint8_t)((nibble_0 << 4) | nibble_1);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_HexToBytes(const char* hex, size_t hex_length, uint8_t* bytes)
{
    // a length of 0 means a null-terminated string
    if (hex_length == 0) {
        hex_length = strlen(hex);
    }

    // check that we have a multiple of 2 characters
    if ((hex_length % 2) != 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // decode
    size_t byte_count = hex_length / 2;
    for (size_t i = 0; i < byte_count; i++) {
        GG_Result result = GG_HexToByte(hex + (i * 2), &bytes[i]);
        if (GG_FAILED(result)) return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
size_t
GG_ProtobufVarintSize(uint64_t value)
{
    size_t size = 1;
    while (value > 127) {
        ++size;
        value >>= 7;
    }

    return size;
}

//----------------------------------------------------------------------
size_t
GG_EncodeProtobufVarint(uint64_t value, uint8_t* encoded)
{
    size_t size = 0;
    do {
        *encoded = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value) {
            *encoded |= 0x80;
        }
        ++encoded;
        ++size;
    } while (value);

    return size;
}

//----------------------------------------------------------------------
size_t
GG_DecodeProtobufVarint(const uint8_t* encoded, size_t encoded_size, uint64_t* decoded)
{
    uint64_t value = 0;
    uint8_t x = 0x80;
    size_t size = 0;
    for (size = 0; (size < GG_MIN(10, encoded_size)) && (x & 0x80); size++) {
        x = encoded[size];
        value |= (uint64_t)(x & 0x7F) << (size * 7);
    }

    // write the value if asked by the caller
    if (decoded) {
        *decoded = value;
    }

    return (x & 0x80) ? 0 : size;
}

//----------------------------------------------------------------------
uint64_t
GG_ProtobufSignedToZigZag(int64_t value)
{
    return ((uint64_t)value << 1) ^ (value < 0 ? 0xFFFFFFFFFFFFFFFF : 0);
}

//----------------------------------------------------------------------
int64_t
GG_ProtobufSignedFromZigZag(uint64_t value)
{
    return (value & 1) ? (-(int64_t)(value >> 1) - 1) : (int64_t)(value >> 1);
}
