/**
*
* @file: fb_smo_jerryscript.c
*
* @copyright
* Copyright 2017 by Fitbit, Inc., all rights reserved.
*
* @author Gilles Boccon-Gibod
*
* @date 2017-03-20
*
* @details
*
* SMO JerryScript bindings
*
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

/* JerryScript public API */
#include "jerryscript.h"

/* we have to dig into the JerryScript internals until there's a better way */
#include "ecma-arraybuffer-object.h"
#include "ecma-typedarray-object.h"
#include "ecma-helpers.h"

#include "fb_smo_cbor.h"
#include "fb_smo_jerryscript.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct Fb_JerryCborParserContext {
    jerry_value_t                     container;
    uint32_t                          count;
    char*                             name;
    unsigned int                      name_length;
    struct Fb_JerryCborParserContext* parent;
    Fb_SmoAllocator*                  allocator;
} Fb_JerryCborParserContext;

typedef struct {
    Fb_Smo_CBOR_ParserListener parser_listener;
    Fb_JerryCborParserContext* context;
    jerry_value_t              value;
    bool                       value_is_initialized;
    unsigned int               children_count;
    unsigned int               expect_name;
} Fb_JerryCborDeserializer;

typedef struct {
    Fb_SmoJerryDataSource base;
    const uint8_t*        buffer;
    unsigned int          buffer_size;
    unsigned int          buffer_used;
} Fb_SmoJerryMemoryDataSource;

/*----------------------------------------------------------------------
|   Fb_SmoJerryMemoryDataSource_GetMore
+---------------------------------------------------------------------*/
static unsigned int
Fb_SmoJerryMemoryDataSource_GetMore(Fb_SmoJerryDataSource* _self,
                                    const uint8_t**        buffer,
                                    unsigned int*          bytes_available)
{
    Fb_SmoJerryMemoryDataSource* self = (Fb_SmoJerryMemoryDataSource*)_self;

    *buffer          = self->buffer+self->buffer_used;
    *bytes_available = self->buffer_size-self->buffer_used;

    if (self->buffer_used == 0) {
        return self->buffer_size;
    } else {
        return 0;
    }
}

/*----------------------------------------------------------------------
|   Fb_SmoJerryMemoryDataSource_Advance
+---------------------------------------------------------------------*/
static int
Fb_SmoJerryMemoryDataSource_Advance(Fb_SmoJerryDataSource* _self, unsigned int bytes_used)
{
    Fb_SmoJerryMemoryDataSource* self = (Fb_SmoJerryMemoryDataSource*)_self;

    if (self->buffer_used + bytes_used <= self->buffer_size) {
        self->buffer_used += bytes_used;
    } else {
        return FB_SMO_ERROR_INTERNAL;
    }
    return FB_SMO_SUCCESS;
}

/*----------------------------------------------------------------------
|   Fb_JerryCborParserContext_Push
+---------------------------------------------------------------------*/
static Fb_JerryCborParserContext*
Fb_JerryCborParserContext_Push(Fb_SmoAllocator* allocator, Fb_JerryCborParserContext* context, jerry_value_t container, uint32_t count)
{
    Fb_JerryCborParserContext* new_context = (Fb_JerryCborParserContext*)allocator->allocate_memory(allocator, sizeof(Fb_JerryCborParserContext));
    if (new_context == NULL) return NULL;
    new_context->container   = container;
    new_context->count       = count;
    new_context->parent      = context;
    new_context->name        = NULL;
    new_context->name_length = 0;
    new_context->allocator   = allocator;

    return new_context;
}

/*----------------------------------------------------------------------
|   Fb_JerryCborParserContext_Pop
+---------------------------------------------------------------------*/
static Fb_JerryCborParserContext*
Fb_JerryCborParserContext_Pop(Fb_SmoAllocator* allocator, Fb_JerryCborParserContext* context)
{
    Fb_JerryCborParserContext* parent_context = context->parent;
    if (context->name) {
        allocator->free_memory(allocator, context->name);
    }
    allocator->free_memory(allocator, context);
    return parent_context;
}

/*----------------------------------------------------------------------
|   Fb_JerryCborDeserializer_OnInteger
+---------------------------------------------------------------------*/
static void
Fb_JerryCborDeserializer_OnInteger(Fb_Smo_CBOR_ParserListener* _self, int64_t value)
{
    Fb_JerryCborDeserializer* self = (Fb_JerryCborDeserializer*)_self;
    self->value = jerry_create_number((double)value);
    self->value_is_initialized = true;
}

/*----------------------------------------------------------------------
|   Fb_JerryCborDeserializer_OnFloat
+---------------------------------------------------------------------*/
static void
Fb_JerryCborDeserializer_OnFloat(Fb_Smo_CBOR_ParserListener* _self, double value)
{
    Fb_JerryCborDeserializer* self = (Fb_JerryCborDeserializer*)_self;
    self->value = jerry_create_number(value);
    self->value_is_initialized = true;
}

/*----------------------------------------------------------------------
|   Fb_JerryCborDeserializer_OnSymbol
+---------------------------------------------------------------------*/
static void
Fb_JerryCborDeserializer_OnSymbol(Fb_Smo_CBOR_ParserListener* _self, Fb_SmoSymbol value)
{
    Fb_JerryCborDeserializer* self = (Fb_JerryCborDeserializer*)_self;
    self->value_is_initialized = true;
    switch (value) {
        case FB_SMO_SYMBOL_NULL:
            self->value = jerry_create_null();
            break;

        case FB_SMO_SYMBOL_TRUE:
        case FB_SMO_SYMBOL_FALSE:
            self->value = jerry_create_boolean(value == FB_SMO_SYMBOL_TRUE);
            break;

        case FB_SMO_SYMBOL_UNDEFINED:
            self->value = jerry_create_undefined();
            break;

        default:
            // not expected
            self->value_is_initialized = false;
            break;
    }
}

/*----------------------------------------------------------------------
|   Fb_JerryCborDeserializer_OnString
+---------------------------------------------------------------------*/
static void
Fb_JerryCborDeserializer_OnString(Fb_Smo_CBOR_ParserListener* _self, const char* value, unsigned int value_length)
{
    Fb_JerryCborDeserializer* self = (Fb_JerryCborDeserializer*)_self;
    if (self->expect_name) {
        /* this string is a key, keep a null-terminated copy */
        self->context->name = self->context->allocator->allocate_memory(self->context->allocator, value_length+1);
        if (self->context->name) {
            self->context->name_length = value_length;
            self->context->name[value_length] = 0;
            if (value_length) {
                memcpy(self->context->name, value, value_length);
            }
        }
    } else {
        /* this string is a value */
        self->value = jerry_create_string_sz_from_utf8((const jerry_char_t*)value, value_length);
        self->value_is_initialized = true;
    }
}

/*----------------------------------------------------------------------
|   Fb_JerryCborDeserializer_OnBytes
+---------------------------------------------------------------------*/
static void
Fb_JerryCborDeserializer_OnBytes(Fb_Smo_CBOR_ParserListener* _self, const uint8_t* value, unsigned int value_length)
{
    Fb_JerryCborDeserializer* self = (Fb_JerryCborDeserializer*)_self;

    /* create an ArrayBuffer object */
    /* NOTE: JerryScript allocations don't return NULL, they "crash" on out of memory conditions
       so we don't check the result here */
    ecma_object_t* arraybuffer = ecma_arraybuffer_new_object(value_length);

    /* copy the data into the buffer */
    lit_utf8_byte_t* buffer = ecma_arraybuffer_get_buffer(arraybuffer);
    memcpy(buffer, value, value_length);


    /* return the result as a jerry_value_t */
    self->value = ecma_make_object_value(arraybuffer);
    self->value_is_initialized = true;
}

/*----------------------------------------------------------------------
|   Fb_JerryCborDeserializer_OnArray
+---------------------------------------------------------------------*/
static void
Fb_JerryCborDeserializer_OnArray(Fb_Smo_CBOR_ParserListener* _self, unsigned int entry_count)
{
    Fb_JerryCborDeserializer* self = (Fb_JerryCborDeserializer*)_self;
    self->value = jerry_create_array(entry_count);
    self->children_count = entry_count;
    self->value_is_initialized = true;
}

/*----------------------------------------------------------------------
|   Fb_JerryCborDeserializer_OnObject
+---------------------------------------------------------------------*/
static void
Fb_JerryCborDeserializer_OnObject(Fb_Smo_CBOR_ParserListener* _self, unsigned int entry_count)
{
    Fb_JerryCborDeserializer* self = (Fb_JerryCborDeserializer*)_self;
    self->value = jerry_create_object();
    self->children_count = entry_count;
    self->value_is_initialized = true;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserialize_CBOR_ToJerry
+---------------------------------------------------------------------*/
int
Fb_Smo_Deserialize_CBOR_ToJerry(Fb_SmoAllocator* parser_allocator,
                                const uint8_t*   serialized,
                                unsigned int     serialized_size,
                                jerry_value_t*   value)
{
    Fb_SmoJerryMemoryDataSource memory_source = {
        {
            Fb_SmoJerryMemoryDataSource_GetMore,
            Fb_SmoJerryMemoryDataSource_Advance
        },
        serialized,
        serialized_size
        // the rest is initialized with zeros
    };

    return Fb_Smo_Deserialize_CBOR_ToJerryFromSource(parser_allocator,
                                                     &memory_source.base,
                                                     value);
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserialize_CBOR_ToJerryFromSource
+---------------------------------------------------------------------*/
int
Fb_Smo_Deserialize_CBOR_ToJerryFromSource(Fb_SmoAllocator*       parser_allocator,
                                          Fb_SmoJerryDataSource* data_source,
                                          jerry_value_t*         value)
{
    Fb_JerryCborDeserializer deserializer = {
        {
            Fb_JerryCborDeserializer_OnInteger,
            Fb_JerryCborDeserializer_OnFloat,
            Fb_JerryCborDeserializer_OnSymbol,
            Fb_JerryCborDeserializer_OnString,
            Fb_JerryCborDeserializer_OnBytes,
            Fb_JerryCborDeserializer_OnObject,
            Fb_JerryCborDeserializer_OnArray
        }
        // the rest is initialized with zeros
    };
    Fb_JerryCborParserContext* context              = NULL;
    unsigned int               bytes_available      = 0;
    const uint8_t*             buffer               = NULL;
    bool                       value_is_initialized = false;
    int                        result               = FB_SMO_ERROR_NOT_ENOUGH_DATA;

    /* initialize the returned value to a known value even if an error is returned */
    /* (this makes debugging easier) */
    *value = 0;

    /* setup the parser allocator */
    if (parser_allocator == NULL) {
        parser_allocator = Fb_SmoDefaultAllocator;
    }

    /* parse until the end of the buffer */
    for (;;) {
        unsigned int done = 0;

        /* setup the variables that need to be accessed during callbacks */
        deserializer.context              = context;
        deserializer.value                = 0; // not strictly necessary, but let's avoid uninitialized members
        deserializer.value_is_initialized = false;
        deserializer.children_count       = 0;

        /* decide if we expect to parse a name or an object */
        deserializer.expect_name = (unsigned int)(context &&
                                                  context->name == NULL &&
                                                  jerry_value_is_object(context->container) &&
                                                  !jerry_value_is_array(context->container));

        /* parse the next element */
        for (;;) {
            if (bytes_available) {
                /* try to parse something */
                unsigned int bytes_left = bytes_available;
                result = Fb_Smo_Parse_CBOR(&deserializer.parser_listener, buffer, &bytes_left);
                if (result == FB_SMO_SUCCESS) {
                    unsigned int bytes_consumed = bytes_available-bytes_left;
                    result = data_source->advance(data_source, bytes_consumed);
                    if (result != 0) {
                        result = FB_SMO_ERROR_INTERNAL;
                        break;
                    }
                    bytes_available = bytes_left;
                    buffer         += bytes_consumed;
                    break;
                }
                if (result != FB_SMO_ERROR_NOT_ENOUGH_DATA) {
                    break;
                }
            }

            /* get more data */
            result = data_source->get_more(data_source, &buffer, &bytes_available);
            if (result == 0 || bytes_available == 0) {
                done = 1;
                break;
            }
        }
        if (done) break;

        /* stop now if something went wrong */
        if (result != FB_SMO_SUCCESS) break;

        /* if we just expected a name, we're done */
        if (deserializer.expect_name) {
            /* check if we ran of memory or if the key wasn't a string */
            if (context->name == NULL) {
                if (deserializer.value_is_initialized) {
                    result = FB_SMO_ERROR_INVALID_FORMAT;
                } else {
                    result = FB_SMO_ERROR_OUT_OF_MEMORY;
                }
                break;
            }
            continue;
        }

        /* if no object was created, return an error */
        if (!deserializer.value_is_initialized) {
            result = FB_SMO_ERROR_INVALID_FORMAT;
            break;
        }

        /* the first object seen becomes the root */
        if (!value_is_initialized) {
            value_is_initialized = true;
            *value = jerry_acquire_value(deserializer.value);
        }

        /* add the new object to the current context if we have one */
        if (context) {
            if (jerry_value_is_array(context->container)) {
                jerry_set_property_by_index(context->container, jerry_get_array_length(context->container)-context->count, deserializer.value);
            } else {
                if (context->name) {
                    /* we already have a key, this is a value */
                    jerry_value_t name = jerry_create_string_sz_from_utf8((const jerry_char_t*)context->name, context->name_length);
                    if (jerry_value_is_error(name)) {
                        result = FB_SMO_ERROR_INVALID_FORMAT;
                    } else {
                        jerry_set_property(context->container, name, deserializer.value);
                    }
                    jerry_release_value(name);

                    parser_allocator->free_memory(parser_allocator, context->name);
                    context->name = NULL;
                    context->name_length = 0;

                    if (result != FB_SMO_SUCCESS) {
                        break;
                    }
                } else {
                    result = FB_SMO_ERROR_INVALID_FORMAT;
                    break;
                }
            }

            /* check if the container is complete */
            if (--context->count == 0) {
                context = Fb_JerryCborParserContext_Pop(parser_allocator, context);
            }
        }

        /* push a new context if this is a non-empty container */
        if (jerry_value_is_object(deserializer.value)) {
            /* we instantiated a new non-empty container, this becomes the new context */
            if (deserializer.children_count) {
                Fb_JerryCborParserContext* new_context = Fb_JerryCborParserContext_Push(parser_allocator,
                                                                                        context,
                                                                                        deserializer.value,
                                                                                        deserializer.children_count);
                if (new_context == NULL) {
                    result = FB_SMO_ERROR_OUT_OF_MEMORY;
                    break;
                }
                context = new_context;
            }
        }

        /* we no longer need the current value */
        jerry_release_value(deserializer.value);

        /* we're done unless we're within a container */
        if (context == NULL) {
            break;
        }
    }

    /* cleanup in case we broke out */
    if (result != FB_SMO_SUCCESS && deserializer.value_is_initialized) {
        jerry_release_value(deserializer.value);
    }

    /* check the structure we have built */
    if (result == FB_SMO_SUCCESS && context) {
        /* the object tree was incomplete */
        result = FB_SMO_ERROR_INVALID_FORMAT;
    }

    if (result != FB_SMO_SUCCESS) {
        /* if we failed, we need to release any partial state */
        if (value_is_initialized) {
            jerry_release_value(*value);
        }
    }

    /* cleanup any remaining context stack */
    while (context) {
        context = Fb_JerryCborParserContext_Pop(parser_allocator, context);
    }

    /* done */
    return result;
}

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static bool
Fb_Smo_JerryForeachSerialize(const jerry_value_t property_name,
                             const jerry_value_t property_value,
                             void*               self);

typedef struct {
    uint8_t**                     serialized;
    unsigned int*                 size;
    unsigned int                  max_depth;
    int                           status;
    Fb_Smo_Serialize_Cbor_Encoder encoder;
    void*                         encoder_context;
} Fb_Smo_JerryForeachSerializeContext;

static bool
Fb_Smo_JerryForeachCount(const jerry_value_t property_name,
                         const jerry_value_t property_value,
                         void*               self);

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_FromJerry_
+---------------------------------------------------------------------*/
static int
Fb_Smo_Serialize_CBOR_FromJerry_(jerry_value_t                 object,
                                 uint8_t**                     serialized,
                                 unsigned int*                 size,
                                 unsigned int                  max_depth,
                                 Fb_Smo_Serialize_Cbor_Encoder encoder,
                                 void*                         encoder_context)
{
    int result = FB_SMO_SUCCESS;
    if (encoder) {
        if (encoder(encoder_context, object, serialized, size, &result)) {
            return result;
        }
    }

    if (jerry_value_is_object(object)) {
        /* check for ArrayBuffer and TypedArray first, since they are objects, but not containers */
        bool is_arraybuffer = ecma_is_arraybuffer(object);
        bool is_typedarray  = ecma_is_typedarray(object);
        if (is_arraybuffer || is_typedarray) {
            ecma_object_t*   ecma_object = ecma_get_object_from_value(object);
            lit_utf8_byte_t* buffer;
            ecma_length_t    buffer_length;

            if (is_arraybuffer) {
                buffer        = ecma_arraybuffer_get_buffer(ecma_object);
                buffer_length = ecma_arraybuffer_get_length(ecma_object);
            } else {
                buffer        = ecma_typedarray_get_buffer(ecma_object);
                buffer_length = ecma_typedarray_get_length(ecma_object) << ecma_typedarray_get_element_size_shift(ecma_object);
            }
            result = Fb_Smo_Serialize_CBOR_Bytes(serialized, size, (const uint8_t*)buffer, (unsigned int)buffer_length);
        } else {
            if (max_depth == 0) {
                return FB_SMO_ERROR_OVERFLOW;
            }
            if (jerry_value_is_array(object)) {
                uint32_t array_length = jerry_get_array_length(object);
                uint32_t i;

                result = Fb_Smo_Serialize_CBOR_Array(serialized, size, array_length);
                if (result != FB_SMO_SUCCESS) return result;

                for (i=0; i<array_length; i++) {
                    jerry_value_t item = jerry_get_property_by_index(object, i);

                    result = Fb_Smo_Serialize_CBOR_FromJerry_(item, serialized, size, max_depth-1, encoder, encoder_context);
                    if (result != FB_SMO_SUCCESS) return result;

                    jerry_release_value(item);
                }
            } else {
                Fb_Smo_JerryForeachSerializeContext foreach_context = {
                    serialized,
                    size,
                    max_depth-1,
                    FB_SMO_SUCCESS,
                    encoder,
                    encoder_context,
                };

                // first count the number of properties
                uint32_t property_count = 0;
                if (!jerry_foreach_object_property(object, Fb_Smo_JerryForeachCount, &property_count)) {
                    return FB_SMO_ERROR_INTERNAL;
                }

                // create the container
                result = Fb_Smo_Serialize_CBOR_Object(serialized, size, property_count);
                if (result != FB_SMO_SUCCESS) return result;

                // serialize each property
                if (!jerry_foreach_object_property(object, Fb_Smo_JerryForeachSerialize, &foreach_context)) {
                    return FB_SMO_ERROR_INTERNAL;
                }
                if (foreach_context.status != FB_SMO_SUCCESS) {
                    return foreach_context.status;
                }
            }
        }
    } else if (jerry_value_is_undefined(object)) {
        result = Fb_Smo_Serialize_CBOR_Symbol(serialized, size, FB_SMO_SYMBOL_UNDEFINED);
        if (result != FB_SMO_SUCCESS) return result;
    } else if (jerry_value_is_null(object)) {
        result = Fb_Smo_Serialize_CBOR_Symbol(serialized, size, FB_SMO_SYMBOL_NULL);
        if (result != FB_SMO_SUCCESS) return result;
    } else if (jerry_value_is_boolean(object)) {
        bool value = jerry_get_boolean_value(object);
        result = Fb_Smo_Serialize_CBOR_Symbol(serialized, size, value?FB_SMO_SYMBOL_TRUE:FB_SMO_SYMBOL_FALSE);
        if (result != FB_SMO_SUCCESS) return result;
    } else if (jerry_value_is_number(object)) {
        double value = jerry_get_number_value(object);
        int64_t int_value = (int64_t)value;
        if ((double)int_value == value) {
            result = Fb_Smo_Serialize_CBOR_Integer(serialized, size, int_value);
            if (result != FB_SMO_SUCCESS) return result;
        } else {
            result = Fb_Smo_Serialize_CBOR_Float(serialized, size, value);
            if (result != FB_SMO_SUCCESS) return result;
        }
    } else if (jerry_value_is_string(object)) {
        // get the size of the UTF-8 representation of the string
        jerry_char_t*  utf8_buffer;
        jerry_length_t utf8_size = jerry_get_utf8_string_size(object);

        // reserve space for the string
        result = Fb_Smo_Serialize_CBOR_StringPrep(serialized, size, utf8_size, &utf8_buffer);
        if (result != FB_SMO_SUCCESS) {
            return result;
        }

        // convert the string into the space reserved
        if (utf8_buffer) {
            jerry_string_to_utf8_char_buffer(object, utf8_buffer, utf8_size);
        }
    } else {
        // not supported
        return FB_SMO_ERROR_NOT_SUPPORTED;
    }

    return FB_SMO_SUCCESS;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize_CBOR_FromJerry
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize_CBOR_FromJerry(jerry_value_t                 object,
                                uint8_t*                      serialized,
                                unsigned int*                 size,
                                unsigned int                  max_depth,
                                Fb_Smo_Serialize_Cbor_Encoder encoder,
                                void*                         encoder_context)
{
    return Fb_Smo_Serialize_CBOR_FromJerry_(object, &serialized, size, max_depth, encoder, encoder_context);
}

/*----------------------------------------------------------------------
|   Fb_Smo_JerryForeachSerialize
+---------------------------------------------------------------------*/
static bool
Fb_Smo_JerryForeachSerialize(const jerry_value_t property_name,
                             const jerry_value_t property_value,
                             void*               self)
{
    Fb_Smo_JerryForeachSerializeContext* context = (Fb_Smo_JerryForeachSerializeContext*)self;
    int result;

    result = Fb_Smo_Serialize_CBOR_FromJerry_(property_name,
                                              context->serialized,
                                              context->size,
                                              context->max_depth,
                                              context->encoder,
                                              context->encoder_context);
    if (result != FB_SMO_SUCCESS) {
        context->status = result;
        return false;
    }

    result = Fb_Smo_Serialize_CBOR_FromJerry_(property_value,
                                              context->serialized,
                                              context->size,
                                              context->max_depth,
                                              context->encoder,
                                              context->encoder_context);
    if (result != FB_SMO_SUCCESS) {
        context->status = result;
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------
|   Fb_Smo_JerryForeachCount
+---------------------------------------------------------------------*/
static bool
Fb_Smo_JerryForeachCount(const jerry_value_t property_name,
                         const jerry_value_t property_value,
                         void*               self)
{
    uint32_t* counter = (uint32_t*)self;
    ++*counter;

    return true;
}


