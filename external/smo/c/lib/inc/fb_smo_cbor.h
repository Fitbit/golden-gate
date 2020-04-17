/**
*
* @file: fb_cbor.c
*
* @copyright
* Copyright 2016-2020 Fitbit, Inc
* SPDX-License-Identifier: Apache-2.0
*
* @author Gilles Boccon-Gibod
*
* @date 2016-11-05
*
* @details
*
* CBOR serializer/deserializer
*
*/

#ifndef FB_SMO_CBOR_H
#define FB_SMO_CBOR_H

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "fb_smo.h"

/*----------------------------------------------------------------------
|    types
+---------------------------------------------------------------------*/
typedef struct Fb_Smo_CBOR_ParserListener Fb_Smo_CBOR_ParserListener;
struct Fb_Smo_CBOR_ParserListener {
    void (*on_integer)(Fb_Smo_CBOR_ParserListener* self, int64_t value);
    void (*on_float)(Fb_Smo_CBOR_ParserListener* self, double value);
    void (*on_symbol)(Fb_Smo_CBOR_ParserListener* self, Fb_SmoSymbol value);
    void (*on_string)(Fb_Smo_CBOR_ParserListener* self, const char* value, unsigned int value_length);
    void (*on_bytes)(Fb_Smo_CBOR_ParserListener* self, const uint8_t* value, unsigned int value_length);
    void (*on_object)(Fb_Smo_CBOR_ParserListener* self, unsigned int entry_count);
    void (*on_array)(Fb_Smo_CBOR_ParserListener* self, unsigned int entry_count);
};

/*----------------------------------------------------------------------
|    prototypes
+---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int Fb_Smo_Serialize_CBOR(Fb_Smo* self, uint8_t* serialized, unsigned int* size);
int Fb_Smo_Deserialize_CBOR(Fb_SmoAllocator* object_allocator,
                            Fb_SmoAllocator* parser_allocator,
                            const uint8_t*   serialized,
                            unsigned int     size,
                            Fb_Smo**         smo);
int Fb_Smo_Parse_CBOR(Fb_Smo_CBOR_ParserListener* listener,
                      const uint8_t*              serialized,
                      unsigned int*               size);

// low-level CBOR serialization
int Fb_Smo_Serialize_CBOR_Object(uint8_t** serialized, unsigned int* available, unsigned int element_count);
int Fb_Smo_Serialize_CBOR_Array(uint8_t** serialized, unsigned int* available, unsigned int attribute_count);
int Fb_Smo_Serialize_CBOR_String(uint8_t** serialized, unsigned int* available, const char* value, unsigned int value_size);
int Fb_Smo_Serialize_CBOR_Bytes(uint8_t** serialized, unsigned int* available, const uint8_t* value, unsigned int value_size);
int Fb_Smo_Serialize_CBOR_Integer(uint8_t** serialized, unsigned int* available, int64_t value);
int Fb_Smo_Serialize_CBOR_Float(uint8_t** serialized, unsigned int* available, double value);
int Fb_Smo_Serialize_CBOR_Symbol(uint8_t** serialized, unsigned int* available, Fb_SmoSymbol value);

// serialization helpers
/**
 * Perform the same serialization logic as for Fb_Smo_Serialize_CBOR_String, but without copying any characters.
 * Instead, a pointer to the string buffer is returned, so that the characters can be copied by the caller.
 */
int Fb_Smo_Serialize_CBOR_StringPrep(uint8_t** serialized, unsigned int* available, unsigned int value_size, uint8_t** string_buffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FB_SMO_CBOR_H */
