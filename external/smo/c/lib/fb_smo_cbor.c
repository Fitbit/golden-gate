/**
*
* @file: fb_smo_cbor.c
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

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include "fb_smo_cbor.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define FB_CBOR_MAJOR_TYPE_POSITIVE_INTEGER 0
#define FB_CBOR_MAJOR_TYPE_NEGATIVE_INTEGER 1
#define FB_CBOR_MAJOR_TYPE_BYTE_STRING      2
#define FB_CBOR_MAJOR_TYPE_TEXT_STRING      3
#define FB_CBOR_MAJOR_TYPE_ARRAY            4
#define FB_CBOR_MAJOR_TYPE_MAP              5
#define FB_CBOR_MAJOR_TYPE_TAG              6
#define FB_CBOR_MAJOR_TYPE_SIMPLE_AND_FLOAT 7

#define FB_CBOR_SIMPLE_VALUE_FALSE          20
#define FB_CBOR_SIMPLE_VALUE_TRUE           21
#define FB_CBOR_SIMPLE_VALUE_NULL           22
#define FB_CBOR_SIMPLE_VALUE_UNDEFINED      23

#define FB_CBOR_FLOAT_16                    25
#define FB_CBOR_FLOAT_32                    26
#define FB_CBOR_FLOAT_64                    27

#define FB_CBOR_ADDITIONAL_INFO_1_BYTE      24
#define FB_CBOR_ADDITIONAL_INFO_2_BYTES     25
#define FB_CBOR_ADDITIONAL_INFO_4_BYTES     26
#define FB_CBOR_ADDITIONAL_INFO_8_BYTES     27

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct Fb_CborParsingContext {
    Fb_Smo*                       smo;
    uint32_t                      count;
    const char*                   name;
    unsigned int                  name_length;
    struct Fb_CborParsingContext* parent;
} Fb_CborParsingContext;

typedef struct {
    Fb_Smo_CBOR_ParserListener parser_listener;
    Fb_SmoAllocator*           object_allocator;
    Fb_CborParsingContext*     context;
    Fb_Smo*                    smo;
    unsigned int               children_count;
    int                        expect_name;
} Fb_CborDeserializer;

/*----------------------------------------------------------------------
|   Fb_Smo_Parse_CBOR
+---------------------------------------------------------------------*/
int
Fb_Smo_Parse_CBOR(Fb_Smo_CBOR_ParserListener* listener,
                  const uint8_t*              serialized,
                  unsigned int*               size)
{
    uint8_t      x;
    unsigned int major_type;
    unsigned int additional_info;
    union {
        uint64_t i;
        double   f;
    } int_or_float_value;
    uint32_t int32_value;

    /* we need at least one byte */
    if (*size < 1) return FB_SMO_ERROR_NOT_ENOUGH_DATA;

    /* read a byte to know the type of the next item */
    x               = *serialized++;
    major_type      = x>>5;
    additional_info = x & 0x1F;

    /* deal with the value encoding */
    --*size;
    int_or_float_value.i = 0;
    switch (additional_info) {
      case FB_CBOR_ADDITIONAL_INFO_1_BYTE: /* 1 byte  */
        if (*size < 1) {
            return FB_SMO_ERROR_NOT_ENOUGH_DATA;
        } else {
            int_or_float_value.i = *serialized++;
            --*size;
        }
        break;

      case FB_CBOR_ADDITIONAL_INFO_2_BYTES: /* 2 bytes */
        if (*size < 2) {
            return FB_SMO_ERROR_NOT_ENOUGH_DATA;
        } else {
            int_or_float_value.i = (uint64_t)(
                                       ((uint16_t)(serialized[0]) << 8) |
                                       ((uint16_t)(serialized[1])     )
                                   );
            serialized += 2;
            *size      -= 2;
        }
        break;

      case FB_CBOR_ADDITIONAL_INFO_4_BYTES: /* 4 bytes */
        if (*size < 4) {
            return FB_SMO_ERROR_NOT_ENOUGH_DATA;
        } else {
            int_or_float_value.i = ((uint32_t)(serialized[0]) << 24) |
                                    ((uint32_t)(serialized[1]) << 16) |
                                    ((uint32_t)(serialized[2]) <<  8) |
                                    ((uint32_t)(serialized[3])      );

            serialized += 4;
            *size      -= 4;
        }
        break;

      case FB_CBOR_ADDITIONAL_INFO_8_BYTES: /* 8 bytes */
        if (*size < 8) {
            return FB_SMO_ERROR_NOT_ENOUGH_DATA;
        } else {
            int_or_float_value.i = ((uint64_t)(serialized[0]) << 56) |
                                    ((uint64_t)(serialized[1]) << 48) |
                                    ((uint64_t)(serialized[2]) << 40) |
                                    ((uint64_t)(serialized[3]) << 32) |
                                    ((uint64_t)(serialized[4]) << 24) |
                                    ((uint64_t)(serialized[5]) << 16) |
                                    ((uint64_t)(serialized[6]) <<  8) |
                                    ((uint64_t)(serialized[7])      );
            serialized += 8;
            *size      -= 8;
        }
        break;

      case 28: /* reserved */
      case 29: /* reserved */
      case 30: /* reserved */
      case 31: /* indefinite length */
        return FB_SMO_ERROR_NOT_SUPPORTED;

      default: /* immediate value */
        int_or_float_value.i = additional_info;
        break;
    }

    /* some elements only use 32-bit values */
    int32_value = (uint32_t)int_or_float_value.i;

    /* decide what object to create based on the major type */
    switch (major_type) {
      case FB_CBOR_MAJOR_TYPE_POSITIVE_INTEGER:
        /* we don't support the entire 64-bit range */
        if ((int64_t)int_or_float_value.i < 0) {
            return FB_SMO_ERROR_OVERFLOW;
        }
        listener->on_integer(listener, (int64_t)int_or_float_value.i);
        break;

      case FB_CBOR_MAJOR_TYPE_NEGATIVE_INTEGER:
        /* we don't support the entire 64-bit range */
        if (~(int64_t)int_or_float_value.i >= 0) {
            return FB_SMO_ERROR_OVERFLOW;
        }
        listener->on_integer(listener, ~(int64_t)int_or_float_value.i);
        break;

      case FB_CBOR_MAJOR_TYPE_BYTE_STRING:
        if (*size < int_or_float_value.i) {
            return FB_SMO_ERROR_NOT_ENOUGH_DATA;
        }
        if (int_or_float_value.i != int32_value) {
            return FB_SMO_ERROR_OVERFLOW;
        }
        listener->on_bytes(listener, serialized, (unsigned int)int32_value);
        serialized += int32_value;
        *size      -= int32_value;
        break;

      case FB_CBOR_MAJOR_TYPE_TEXT_STRING:
        if (*size < int_or_float_value.i) {
            return FB_SMO_ERROR_NOT_ENOUGH_DATA;
        }
        if (int_or_float_value.i != int32_value) {
            return FB_SMO_ERROR_OVERFLOW;
        }
        listener->on_string(listener, int32_value?(const char*)serialized:NULL, (unsigned int)int32_value);
        serialized += int32_value;
        *size      -= int32_value;
        break;

      case FB_CBOR_MAJOR_TYPE_ARRAY:
        if (int_or_float_value.i != int32_value) {
            return FB_SMO_ERROR_OVERFLOW;
        }
        listener->on_array(listener, (unsigned int)int32_value);
        break;

      case FB_CBOR_MAJOR_TYPE_MAP:
        if (int_or_float_value.i != int32_value) {
            return FB_SMO_ERROR_OVERFLOW;
        }
        listener->on_object(listener, (unsigned int)int32_value);
        break;

      case FB_CBOR_MAJOR_TYPE_TAG:
        return FB_SMO_ERROR_NOT_SUPPORTED;

      case FB_CBOR_MAJOR_TYPE_SIMPLE_AND_FLOAT:
        switch (additional_info) {
          case FB_CBOR_FLOAT_32:
            if (int_or_float_value.i != int32_value) {
                return FB_SMO_ERROR_OVERFLOW;
            } else {
                union {
                    float    f;
                    uint32_t i;
                } int_or_float_value_32;
                int_or_float_value_32.i = int32_value;
                listener->on_float(listener, int_or_float_value_32.f);
            }
            break;

          case FB_CBOR_FLOAT_64:
            listener->on_float(listener, int_or_float_value.f);
            break;

          case FB_CBOR_SIMPLE_VALUE_NULL:
            listener->on_symbol(listener, FB_SMO_SYMBOL_NULL);
            break;

          case FB_CBOR_SIMPLE_VALUE_TRUE:
            listener->on_symbol(listener, FB_SMO_SYMBOL_TRUE);
            break;

          case FB_CBOR_SIMPLE_VALUE_FALSE:
            listener->on_symbol(listener, FB_SMO_SYMBOL_FALSE);
            break;

          case FB_CBOR_SIMPLE_VALUE_UNDEFINED:
            listener->on_symbol(listener, FB_SMO_SYMBOL_UNDEFINED);
            break;

          default:
            return FB_SMO_ERROR_NOT_SUPPORTED;
        }
        break;

      default:
        return FB_SMO_ERROR_INTERNAL;
    }

    /* done */
    return FB_SMO_SUCCESS;
}

/*----------------------------------------------------------------------
|   Fb_CborParsingContext_Push
+---------------------------------------------------------------------*/
static Fb_CborParsingContext*
Fb_CborParsingContext_Push(Fb_SmoAllocator* allocator, Fb_CborParsingContext* context, Fb_Smo* smo, uint32_t count)
{
    Fb_CborParsingContext* new_context = (Fb_CborParsingContext*)allocator->allocate_memory(allocator, sizeof(Fb_CborParsingContext));
    if (new_context == NULL) return NULL;
    new_context->smo         = smo;
    new_context->count       = count;
    new_context->parent      = context;
    new_context->name        = NULL;
    new_context->name_length = 0;

    return new_context;
}

/*----------------------------------------------------------------------
|   Fb_CborParsingContext_Pop
+---------------------------------------------------------------------*/
static Fb_CborParsingContext*
Fb_CborParsingContext_Pop(Fb_SmoAllocator* allocator, Fb_CborParsingContext* context)
{
    Fb_CborParsingContext* parent_context = context->parent;
    allocator->free_memory(allocator, context);
    return parent_context;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserializer_OnInteger
+---------------------------------------------------------------------*/
static void
Fb_Smo_Deserializer_OnInteger(Fb_Smo_CBOR_ParserListener* _self, int64_t value)
{
    Fb_CborDeserializer* self = (Fb_CborDeserializer*)_self;
    self->smo = Fb_Smo_CreateInteger(self->object_allocator, value);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserializer_OnFloat
+---------------------------------------------------------------------*/
static void
Fb_Smo_Deserializer_OnFloat(Fb_Smo_CBOR_ParserListener* _self, double value)
{
    Fb_CborDeserializer* self = (Fb_CborDeserializer*)_self;
    self->smo = Fb_Smo_CreateFloat(self->object_allocator, value);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserializer_OnSymbol
+---------------------------------------------------------------------*/
static void
Fb_Smo_Deserializer_OnSymbol(Fb_Smo_CBOR_ParserListener* _self, Fb_SmoSymbol value)
{
    Fb_CborDeserializer* self = (Fb_CborDeserializer*)_self;
    self->smo = Fb_Smo_CreateSymbol(self->object_allocator, value);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserializer_OnString
+---------------------------------------------------------------------*/
static void
Fb_Smo_Deserializer_OnString(Fb_Smo_CBOR_ParserListener* _self, const char* value, unsigned int value_length)
{
    Fb_CborDeserializer* self = (Fb_CborDeserializer*)_self;
    if (self->expect_name) {
        /* this string is a key */
        self->context->name        = value_length?value:"";
        self->context->name_length = value_length;
    } else {
        /* this string is a value */
        self->smo = Fb_Smo_CreateString(self->object_allocator, value, value_length);
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserializer_OnBytes
+---------------------------------------------------------------------*/
static void
Fb_Smo_Deserializer_OnBytes(Fb_Smo_CBOR_ParserListener* _self, const uint8_t* value, unsigned int value_length)
{
    Fb_CborDeserializer* self = (Fb_CborDeserializer*)_self;
    self->smo = Fb_Smo_CreateBytes(self->object_allocator, value, value_length);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserializer_OnArray
+---------------------------------------------------------------------*/
static void
Fb_Smo_Deserializer_OnArray(Fb_Smo_CBOR_ParserListener* _self, unsigned int entry_count)
{
    Fb_CborDeserializer* self = (Fb_CborDeserializer*)_self;
    self->smo = Fb_Smo_CreateArray(self->object_allocator);
    self->children_count = entry_count;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserializer_OnObject
+---------------------------------------------------------------------*/
static void
Fb_Smo_Deserializer_OnObject(Fb_Smo_CBOR_ParserListener* _self, unsigned int entry_count)
{
    Fb_CborDeserializer* self = (Fb_CborDeserializer*)_self;
    self->smo = Fb_Smo_CreateObject(self->object_allocator);
    self->children_count = entry_count;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserialize_CBOR
+---------------------------------------------------------------------*/
int
Fb_Smo_Deserialize_CBOR(Fb_SmoAllocator* object_allocator,
                        Fb_SmoAllocator* parser_allocator,
                        const uint8_t*   serialized,
                        unsigned int     size,
                        Fb_Smo**         smo)
{
    Fb_CborDeserializer deserializer = {
        {
            Fb_Smo_Deserializer_OnInteger,
            Fb_Smo_Deserializer_OnFloat,
            Fb_Smo_Deserializer_OnSymbol,
            Fb_Smo_Deserializer_OnString,
            Fb_Smo_Deserializer_OnBytes,
            Fb_Smo_Deserializer_OnObject,
            Fb_Smo_Deserializer_OnArray
        },
        (((object_allocator)?(object_allocator):Fb_SmoDefaultAllocator)),
        NULL, NULL, 0, 0
    };
    Fb_CborParsingContext* context = NULL;
    unsigned int bytes_left        = size;
    int          result            = FB_SMO_ERROR_NOT_ENOUGH_DATA;

    /* setup the default return value */
    *smo = NULL;

    /* setup the parser allocator */
    if (parser_allocator == NULL) {
        parser_allocator = Fb_SmoDefaultAllocator;
    }

    /* parse until the end of the buffer */
    while (bytes_left) {
        /* setup the variables that need to be accessed during callbacks */
        deserializer.context        = context;
        deserializer.smo            = NULL;
        deserializer.children_count = 0;

        /* decide if we expect to parse a name or an object */
        deserializer.expect_name = (int)(context && Fb_Smo_GetType(context->smo) == FB_SMO_TYPE_OBJECT && context->name == NULL);

        /* parse the next element */
        result = Fb_Smo_Parse_CBOR(&deserializer.parser_listener, serialized+size-bytes_left, &bytes_left);

        /* stop now if something went wrong */
        if (result != FB_SMO_SUCCESS) break;

        /* if we just expecte a name, we're done */
        if (deserializer.expect_name) {
            continue;
        }

        /* if no object was created, it had to be an out-of-memory error */
        if (deserializer.smo == NULL) {
            result = FB_SMO_ERROR_OUT_OF_MEMORY;
            break;
        }

        /* the first object seen becomes the root */
        if (*smo == NULL) {
            *smo = deserializer.smo;
        }

        /* add the new object to the current context if we have one */
        if (context) {
            Fb_SmoType context_type = Fb_Smo_GetType(context->smo);

            result = Fb_Smo_AddChild(context->smo, context->name, context->name_length, deserializer.smo);
            if (result != FB_SMO_SUCCESS) {
                break;
            }

            if (context_type == FB_SMO_TYPE_OBJECT) {
                if (context->name) {
                    /* we already have a key, this is a value */
                    context->name = NULL;
                    context->name_length = 0;
                } else {
                    result = FB_SMO_ERROR_INVALID_FORMAT;
                    break;
                }
            }

            /* check if the container is complete */
            if (--context->count == 0) {
                context = Fb_CborParsingContext_Pop(parser_allocator, context);
            }
        }

        /* push a new context if this is a non-empty container */
        {
            Fb_SmoType smo_type = Fb_Smo_GetType(deserializer.smo);
            if (smo_type == FB_SMO_TYPE_ARRAY || smo_type == FB_SMO_TYPE_OBJECT) {
                /* we instantiated a new non-empty container, this becomes the new context */
                if (deserializer.children_count) {
                    Fb_CborParsingContext* new_context = Fb_CborParsingContext_Push(parser_allocator,
                                                                                    context,
                                                                                    deserializer.smo,
                                                                                    deserializer.children_count);
                    if (new_context == NULL) {
                        result = FB_SMO_ERROR_OUT_OF_MEMORY;
                        break;
                    }
                    context = new_context;
                }
            }
        }

        /* we're done unless we're within a container */
        if (context == NULL) {
            break;
        }
    }

    /* check the structure we have built */
    if (result == FB_SMO_SUCCESS && context) {
        /* the object tree was incomplete */
        result = FB_SMO_ERROR_INVALID_FORMAT;
    }

    if (result != FB_SMO_SUCCESS) {
        /* if we failed, we need to destroy any partial state */
        if (*smo) {
            Fb_Smo_Destroy(*smo);
            *smo = NULL;
        }
    }

    /* cleanup any remaining context stack */
    while (context) {
        context = Fb_CborParsingContext_Pop(parser_allocator, context);
    }

    /* done */
    return result;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CBOR_PackInfo
+---------------------------------------------------------------------*/
static int
Fb_Smo_CBOR_PackInfo(uint8_t** serialized, unsigned int* available, uint8_t type, int64_t length)
{
    uint8_t      bytes[9];
    unsigned int bytes_size;
    int          result = FB_SMO_SUCCESS;

    bytes[0] = (uint8_t)(type << 5);
    if (length < 24) {
        if (*serialized) {
            bytes[0] |= length;
        }
        bytes_size = 1;
    } else if (length < 256) {
        if (*serialized) {
            bytes[0] |= FB_CBOR_ADDITIONAL_INFO_1_BYTE;
            bytes[1]  = (uint8_t)length;
        }
        bytes_size = 2;
    } else if (length <= 0xFFFF) {
        if (*serialized) {
            bytes[0] |= FB_CBOR_ADDITIONAL_INFO_2_BYTES;
            bytes[1] = (uint8_t)(((uint16_t)length) >> 8);
            bytes[2] = (uint8_t)(((uint16_t)length)     );
        }
        bytes_size = 3;
    } else if (length <= 0xFFFFFFFF) {
        if (*serialized) {
            bytes[0] |= FB_CBOR_ADDITIONAL_INFO_4_BYTES;
            bytes[1] = (uint8_t)(((uint32_t)length) >> 24);
            bytes[2] = (uint8_t)(((uint32_t)length) >> 16);
            bytes[3] = (uint8_t)(((uint32_t)length) >>  8);
            bytes[4] = (uint8_t)(((uint32_t)length)      );
        }
        bytes_size = 5;
    } else {
        if (*serialized) {
            bytes[0] |= FB_CBOR_ADDITIONAL_INFO_8_BYTES;
            bytes[1] = (uint8_t)(((uint64_t)length) >> 56);
            bytes[2] = (uint8_t)(((uint64_t)length) >> 48);
            bytes[3] = (uint8_t)(((uint64_t)length) >> 40);
            bytes[4] = (uint8_t)(((uint64_t)length) >> 32);
            bytes[5] = (uint8_t)(((uint64_t)length) >> 24);
            bytes[6] = (uint8_t)(((uint64_t)length) >> 16);
            bytes[7] = (uint8_t)(((uint64_t)length) >>  8);
            bytes[8] = (uint8_t)(((uint64_t)length)      );
        }
        bytes_size = 9;
    }

    if (*serialized) {
        if (*available < bytes_size) {
            result = FB_SMO_ERROR_NOT_ENOUGH_SPACE;
        } else {
            memcpy(*serialized, bytes, bytes_size);
            *serialized += bytes_size;
            *available  -= bytes_size;
        }
    } else {
        *available += bytes_size;
    }

    return result;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CBOR_PackBytes
+---------------------------------------------------------------------*/
static int
Fb_Smo_CBOR_PackBytes(uint8_t** serialized, unsigned int* available, const uint8_t* data, unsigned int data_size)
{
    int result = FB_SMO_SUCCESS;

    if (*serialized) {
        if (*available < data_size) {
            result = FB_SMO_ERROR_NOT_ENOUGH_SPACE;
        } else if (data_size) {
            if (data) {
                memcpy(*serialized, data, data_size);
            }
            *serialized += data_size;
            *available  -= data_size;
        }
    } else {
        *available += data_size;
    }

    return result;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CBOR_PackDirect
+---------------------------------------------------------------------*/
static int
Fb_Smo_CBOR_PackDirect(uint8_t** serialized, unsigned int* available, uint8_t type, uint8_t value)
{
    int result = FB_SMO_SUCCESS;

    if (*serialized) {
        if (*available < 1) {
            result = FB_SMO_ERROR_NOT_ENOUGH_SPACE;
        } else {
            **serialized = (uint8_t)(type << 5) | value;
            *serialized += 1;
            *available  -= 1;
        }
    } else {
        *available += 1;
    }

    return result;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_Object
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_Object(uint8_t** serialized, unsigned int* available, unsigned int attribute_count)
{
    return Fb_Smo_CBOR_PackInfo(serialized, available, FB_CBOR_MAJOR_TYPE_MAP, attribute_count);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_Array
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_Array(uint8_t** serialized, unsigned int* available, unsigned int element_count)
{
    return Fb_Smo_CBOR_PackInfo(serialized, available, FB_CBOR_MAJOR_TYPE_ARRAY, element_count);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_String
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_String(uint8_t** serialized, unsigned int* available, const char* value, unsigned int value_size)
{
    int result;

    if (value_size == 0 && value) {
        value_size = (unsigned int)strlen(value);
    }
    result = Fb_Smo_CBOR_PackInfo(serialized, available, FB_CBOR_MAJOR_TYPE_TEXT_STRING, value_size);
    if (result != FB_SMO_SUCCESS) return result;
    return Fb_Smo_CBOR_PackBytes(serialized, available, (const uint8_t*)value, value_size);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_StringPrep
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_StringPrep(uint8_t** serialized, unsigned int* available, unsigned int value_size, uint8_t** string_buffer)
{
    int result;

    result = Fb_Smo_CBOR_PackInfo(serialized, available, FB_CBOR_MAJOR_TYPE_TEXT_STRING, value_size);
    if (result != FB_SMO_SUCCESS) return result;

    /* remember the current buffer position */
    *string_buffer = *serialized;
    return Fb_Smo_CBOR_PackBytes(serialized, available, NULL, value_size);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_Bytes
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_Bytes(uint8_t** serialized, unsigned int* available, const uint8_t* value, unsigned int value_size)
{
    int result = Fb_Smo_CBOR_PackInfo(serialized, available, FB_CBOR_MAJOR_TYPE_BYTE_STRING, value_size);
    if (result != FB_SMO_SUCCESS) return result;
    return Fb_Smo_CBOR_PackBytes(serialized, available, value, value_size);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_Integer
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_Integer(uint8_t** serialized, unsigned int* available, int64_t value)
{
    if (value >= 0) {
        return Fb_Smo_CBOR_PackInfo(serialized, available, FB_CBOR_MAJOR_TYPE_POSITIVE_INTEGER, value);
    } else {
        return Fb_Smo_CBOR_PackInfo(serialized, available, FB_CBOR_MAJOR_TYPE_NEGATIVE_INTEGER, -(value+1));
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_Float
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_Float(uint8_t** serialized, unsigned int* available, double value)
{
    union {
        double   f;
        uint64_t i;
    } value_u = { value };

    int result = Fb_Smo_CBOR_PackDirect(serialized, available, FB_CBOR_MAJOR_TYPE_SIMPLE_AND_FLOAT, FB_CBOR_FLOAT_64);
    if (result == FB_SMO_SUCCESS) {
        uint8_t bytes[8];
        bytes[0] = (uint8_t)(value_u.i >> 56);
        bytes[1] = (uint8_t)(value_u.i >> 48);
        bytes[2] = (uint8_t)(value_u.i >> 40);
        bytes[3] = (uint8_t)(value_u.i >> 32);
        bytes[4] = (uint8_t)(value_u.i >> 24);
        bytes[5] = (uint8_t)(value_u.i >> 16);
        bytes[6] = (uint8_t)(value_u.i >>  8);
        bytes[7] = (uint8_t)(value_u.i      );

        return Fb_Smo_CBOR_PackBytes(serialized, available, bytes, 8);
    }

    return result;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_Symbol
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_Symbol(uint8_t** serialized, unsigned int* available, Fb_SmoSymbol value)
{
    uint8_t cbor_value;
    switch (value) {
      case FB_SMO_SYMBOL_NULL:
        cbor_value = FB_CBOR_SIMPLE_VALUE_NULL;
        break;

      case FB_SMO_SYMBOL_TRUE:
        cbor_value = FB_CBOR_SIMPLE_VALUE_TRUE;
        break;

      case FB_SMO_SYMBOL_FALSE:
        cbor_value = FB_CBOR_SIMPLE_VALUE_FALSE;
        break;

      case FB_SMO_SYMBOL_UNDEFINED:
        cbor_value = FB_CBOR_SIMPLE_VALUE_UNDEFINED;
        break;

      default:
        return FB_SMO_ERROR_INTERNAL;
    }
    return Fb_Smo_CBOR_PackDirect(serialized, available, FB_CBOR_MAJOR_TYPE_SIMPLE_AND_FLOAT, cbor_value);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR
|
|   Serialize a tree (root or subtree).
|   This implementation is non-recursive.
|   We use a 'depth' variable to differentiate between the top level
|   and the rest, because when serializing a subtree, we don't want to
|   serialize the name or the siblings, if any.
|
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR(Fb_Smo* self, uint8_t* serialized, unsigned int* size)
{
    unsigned int available = serialized ? *size : 0;
    unsigned int depth = 0;
    int result;

    /* init the return values */
    while (self) {
        result = FB_SMO_SUCCESS;

        /* output a name if we're in an object */
        if (depth) {
            const char* name = Fb_Smo_GetName(self);
            if (name) {
                result = Fb_Smo_Serialize_CBOR_String(&serialized, &available, name, 0);
                if (result != FB_SMO_SUCCESS) return result;
            }
        }

        /* output an element based on its type */
        switch (Fb_Smo_GetType(self)) {
          case FB_SMO_TYPE_OBJECT:
            result = Fb_Smo_Serialize_CBOR_Object(&serialized, &available, Fb_Smo_GetChildrenCount(self));
            break;

          case FB_SMO_TYPE_ARRAY:
            result = Fb_Smo_Serialize_CBOR_Array(&serialized, &available, Fb_Smo_GetChildrenCount(self));
            break;

          case FB_SMO_TYPE_STRING:
            result = Fb_Smo_Serialize_CBOR_String(&serialized, &available, Fb_Smo_GetValueAsString(self), 0);
            break;

          case FB_SMO_TYPE_BYTES: {
                unsigned int value_size;
                const uint8_t* value = Fb_Smo_GetValueAsBytes(self, &value_size);
                result = Fb_Smo_Serialize_CBOR_Bytes(&serialized, &available, value, value_size);
            }
            break;

          case FB_SMO_TYPE_INTEGER:
            result = Fb_Smo_Serialize_CBOR_Integer(&serialized, &available, Fb_Smo_GetValueAsInteger(self));
            break;

          case FB_SMO_TYPE_FLOAT:
            result = Fb_Smo_Serialize_CBOR_Float(&serialized, &available, Fb_Smo_GetValueAsFloat(self));
            break;

          case FB_SMO_TYPE_SYMBOL:
            result = Fb_Smo_Serialize_CBOR_Symbol(&serialized, &available, Fb_Smo_GetValueAsSymbol(self));
            break;

          default:
            // New CBOR version, perhaps.
            result = FB_SMO_ERROR_INTERNAL;
            break;
        }

        /* check the status of the previous step */
        if (result != FB_SMO_SUCCESS) return result;

        /* move on to the next element in traversal order */
        {
            Fb_Smo* child = Fb_Smo_GetFirstChild(self);
            if (child) {
                /* traverse down */
                ++depth;
                self = child;
            } else {
                do {
                    /* traverse sideways */
                    if (depth) {
                        Fb_Smo* next = Fb_Smo_GetNext(self);
                        if (next) {
                            self = next;
                            break;
                        }
                    }

                    /* traverse back up */
                    if (depth-- == 0) {
                        /* we're back where we started, so we're done */
                        self = NULL;
                        break;
                    } else {
                        self = Fb_Smo_GetParent(self);
                    }
                } while (self);
            }
        }
    }

    /* return the size of the serialized data */
    if (serialized) {
        *size -=available;
    } else {
        *size = available;
    }

    return FB_SMO_SUCCESS;
}
