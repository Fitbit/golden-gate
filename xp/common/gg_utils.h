/**
 * @file
 * @brief General purpose utility functions and macros
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_strings.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//! \addtogroup Utilities
//! General purpose utility functions and macros
//! @{

/*----------------------------------------------------------------------
|   macros and constants
+---------------------------------------------------------------------*/
#define GG_ARRAY_SIZE(x) (sizeof((x))/sizeof((x)[0])) ///< Number of elements in a C array
#define GG_MIN(a, b) (((a) < (b)) ? (a) : (b))        ///< Minimum of two values
#define GG_MAX(a, b) (((a) > (b)) ? (a) : (b))        ///< Maximum of two values
#define GG_MILLISECONDS_PER_SECOND      1000          ///< Number of milliseconds in a second
#define GG_MICROSECONDS_PER_SECOND      1000000       ///< Number of microseconds in a second
#define GG_MICROSECONDS_PER_MILLISECOND 1000          ///< Number of microseconds in a millisecond
#define GG_NANOSECONDS_PER_SECOND       1000000000    ///< Number of nanoseconds in a second
#define GG_NANOSECONDS_PER_MILLISECOND  1000000       ///< Number of nanoseconds in a millisecond
#define GG_NANOSECONDS_PER_MICROSECOND  1000          ///< Number of nanoseconds in a microsecond

#define GG_PROTOBUF_WIRE_TYPE_VARINT           0
#define GG_PROTOBUF_WIRE_TYPE_64_BIT           1
#define GG_PROTOBUF_WIRE_TYPE_LENGTH_DELIMITED 2
#define GG_PROTOBUF_WIRE_TYPE_START_GROUP      3
#define GG_PROTOBUF_WIRE_TYPE_END_GROUP        4
#define GG_PROTOBUF_WIRE_TYPE_32_BIT           5
#define GG_PROTOBUF_FIELD_KEY(field_number, wire_type) (((field_number) << 3) | (GG_PROTOBUF_WIRE_TYPE_##wire_type))

//----------------------------------------------------------------------
//! Interface implemented by simple callback handlers
//!
//! Typically used by objects that can be called back without any parameters.
//!
//! @interface GG_GenericCallbackHandler
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_GenericCallbackHandler) {
    /**
     * Invoked when the object is called back.
     *
     * @param self The object on which this method is invoked.
     */
    void (*OnCallback)(GG_GenericCallbackHandler* self);
};

//! @var GG_GenericCallbackHandler::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_GenericCallbackHandlerInterface
//! Virtual function table for the GG_GenericCallbackHandler interface

//! @relates GG_GenericCallbackHandler
//! @copydoc GG_GenericCallbackHandler::OnCallback
void GG_GenericCallbackHandler_OnCallback(GG_GenericCallbackHandler* self);

//----------------------------------------------------------------------
//! Simple object that implements the GG_GenericCallbackHandler interface
//! and delegates the callback function to a simple function pointer,
//! passing it a custom state pointer.
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_GenericCallbackHandler);
    void (*handler)(void* state); ///< Callback handler function
    void* state;                  ///< Custom state
} GG_SimpleCallback;

/**
 * Initialize a GG_SimpleCallback object.
 *
 * @param callback Pointer to the object to initialize.
 * @param handler Pointer to the handler function for the callback. If this pointer is NULL,
 *                the callback handler will be a no-op.
 * @param state Pointer to the state that will be passed to the callback handler function.
 *
 * @return the GG_GenericCallbackHandler interface for the initialized object.
 */
GG_GenericCallbackHandler* GG_SimpleCallback_Init(GG_SimpleCallback* callback,
                                                  void               (*handler)(void* state),
                                                  void*              state);

/*----------------------------------------------------------------------
|   random
+---------------------------------------------------------------------*/

/**
 * Obtain a 32-bit random value.
 *
 * > NOTE: do not use as a secure source of entropy, this is only
 * for non-secure pseudo random number generation.
 *
 * @return A pseudo-random 32-bit value.
 */
uint32_t GG_GetRandomInteger(void);

/**
 * Obtain an array of random bytes.
 *
 * > NOTE: do not use as a secure source of entropy, this is only
 * for non-secure pseudo random byte generation.
 *
 * @param buffer Memory in which to store the random bytes.
 * @param buffer_size Size of the buffer to fill.
 */
void GG_GetRandomBytes(uint8_t* buffer, size_t buffer_size);

/*----------------------------------------------------------------------
|   base64
+---------------------------------------------------------------------*/
/**
 * Encode a byte array into a base64 string.
 *
 * @param input Input data to encode.
 * @param input_size Input size in bytes.
 * @param output Output buffer.
 * @param output_size Pointer to a variable containing on input the size of the output buffer
 *                    and on output the number of bytes written.
 *                    If the buffer size is too small, the buffer size needed is returned instead.
 * @param url_safe Boolean indicating whether or not to use the URL-safe character set instead of the
 *                 standard one.
 * @return #GG_SUCCESS if the input could be encoded, or #GG_ERROR_NOT_ENOUGH_SPACE if the output
 *         buffer was too small.
 */
GG_Result GG_Base64_Encode(const uint8_t* input,
                           size_t         input_size,
                           char*          output,
                           size_t*        output_size,
                           bool           url_safe);

/**
 * Decode a base64 string into a byte array.
 *
 * @param input Base64 string to decode.
 * @param input_size Length of the input string (if the string is null-terminated, passing 0
 *                   will automatically measure the length of the string).
 * @param output Output buffer.
 * @param output_size Pointer to a variable containing on input the size if the output buffer
 *                    and on output the number of bytes written.
 *                    If the buffer size is too small, the buffer size needed is returned instead.
 * @param url_safe Boolean indicating whether or not to use the URL-safe character set instead of the
 *                 standard one.
 * @return #GG_SUCCESS if the input could be decoded, #GG_ERROR_NOT_ENOUGH_SPACE if the output buffer
 *         was too small, or #GG_ERROR_INVALID_FORMAT if the input is not valid base64.
 */
GG_Result GG_Base64_Decode(const char* input,
                           size_t      input_size,
                           uint8_t*    output,
                           size_t*     output_size,
                           bool        url_safe);

char GG_NibbleToHex(unsigned int nibble, bool uppercase);
void GG_ByteToHex(uint8_t b, char* buffer, bool uppercase);
void GG_BytesToHex(const uint8_t* bytes, size_t bytes_size, char* hex, bool uppercase);
void GG_BytesToHexString(const uint8_t* bytes, size_t bytes_size, GG_String* hex, bool uppercase);
int GG_HexToNibble(char hex);
GG_Result GG_HexToByte(const char* buffer, uint8_t* byte);
GG_Result GG_HexToBytes(const char* hex, size_t hex_length, uint8_t* bytes);

/*----------------------------------------------------------------------
|   bytes <-> integer conversions
+---------------------------------------------------------------------*/
/**
 * Convert a 16-bit integer to 2 bytes, using big-endian byte order.
 *
 * @param buffer Pointer to a 2-byte buffer.
 * @param value Integer value to convert.
 */
void GG_BytesFromInt16Be(uint8_t* buffer, uint16_t value);

/**
 * Convert a 32-bit integer to 4 bytes, using big-endian byte order.
 *
 * @param buffer Pointer to a 4-byte buffer.
 * @param value Integer value to convert.
 */
void GG_BytesFromInt32Be(uint8_t* buffer, uint32_t value);

/**
 * Convert a 64-bit integer to 8 bytes, using big-endian byte order.
 *
 * @param buffer Pointer to an 8-byte buffer.
 * @param value Integer value to convert.
 */
void GG_BytesFromInt64Be(uint8_t* buffer, uint64_t value);

/**
 * Convert a 16-bit integer value to 2 bytes, using big-endian byte order.
 *
 * @param buffer Pointer to a 2-byte buffer.
 * @return The 16-bit integer value.
 */
uint16_t GG_BytesToInt16Be(const uint8_t* buffer);

/**
 * Convert a 32-bit integer value to 4 bytes, using big-endian byte order.
 *
 * @param buffer Pointer to a 4-byte buffer.
 * @return The 32-bit integer value.
 */
uint32_t GG_BytesToInt32Be(const uint8_t* buffer);

/**
 * Convert a 64-bit integer value to 8 bytes, using big-endian byte order.
 *
 * @param buffer Pointer to an 8-byte buffer.
 * @return The 64-bit integer value.
 */
uint64_t GG_BytesToInt64Be(const uint8_t* buffer);

/**
 * Convert a 16-bit integer to 2 bytes, using little-endian byte order.
 *
 * @param buffer Pointer to a 2-byte buffer.
 * @param value Integer value to convert.
 */
void GG_BytesFromInt16Le(uint8_t* buffer, uint16_t value);

/**
 * Convert a 32-bit integer to 4 bytes, using little-endian byte order.
 *
 * @param buffer Pointer to a 4-byte buffer.
 * @param value Integer value to convert.
 */
void GG_BytesFromInt32Le(uint8_t* buffer, uint32_t value);

/**
 * Convert a 64-bit integer to 8 bytes, using big-endian byte order.
 *
 * @param buffer Pointer to an 8-byte buffer.
 * @param value Integer value to convert.
 */
void GG_BytesFromInt64Le(uint8_t* buffer, uint64_t value);

/**
 * Convert a 16-bit integer value to 2 bytes, using little-endian byte order.
 *
 * @param buffer Pointer to a 2-byte buffer.
 * @return The 16-bit integer value.
 */
uint16_t GG_BytesToInt16Le(const uint8_t* buffer);

/**
 * Convert a 32-bit integer value to 4 bytes, using little-endian byte order.
 *
 * @param buffer Pointer to a 4-byte buffer.
 * @return The 32-bit integer value.
 */
uint32_t GG_BytesToInt32Le(const uint8_t* buffer);

/**
 * Convert a 64-bit integer value to 8 bytes, using little-endian byte order.
 *
 * @param buffer Pointer to an 8-byte buffer.
 * @return The 64-bit integer value.
 */
uint64_t GG_BytesToInt64Le(const uint8_t* buffer);

size_t GG_ProtobufVarintSize(uint64_t value);
size_t GG_EncodeProtobufVarint(uint64_t value, uint8_t* encoded);
size_t GG_DecodeProtobufVarint(const uint8_t* encoded, size_t encoded_size, uint64_t* decoded);
uint64_t GG_ProtobufSignedToZigZag(int64_t value);
int64_t GG_ProtobufSignedFromZigZag(uint64_t value);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
