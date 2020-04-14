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
 * Implementation of the GG_Buffer interface.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_types.h"
#include "gg_buffer.h"
#include "gg_port.h"
#include "gg_memory.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static GG_Buffer*
GG_StaticBuffer_Retain(GG_Buffer* self)
{
    return self;
}

static void
GG_StaticBuffer_Release(GG_Buffer* self)
{
    GG_COMPILER_UNUSED(self);
}

static const uint8_t*
GG_StaticBuffer_GetData(const GG_Buffer* self)
{
    return ((const GG_StaticBuffer*)self)->data;
}

static uint8_t*
GG_StaticBuffer_UseData(GG_Buffer* self)
{
    GG_COMPILER_UNUSED(self);

    // static buffers are immutable
    return NULL;
}

static size_t
GG_StaticBuffer_GetDataSize(const GG_Buffer* self)
{
    return ((const GG_StaticBuffer*)self)->data_size;
}

/*----------------------------------------------------------------------
|   function table
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_StaticBuffer, GG_Buffer)
{
    GG_StaticBuffer_Retain,
    GG_StaticBuffer_Release,
    GG_StaticBuffer_GetData,
    GG_StaticBuffer_UseData,
    GG_StaticBuffer_GetDataSize
};

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/
GG_Buffer*
GG_Buffer_Retain(GG_Buffer* self)
{
    GG_ASSERT(self != NULL);
    return GG_INTERFACE(self)->Retain(self);
}

//----------------------------------------------------------------------
void
GG_Buffer_Release(GG_Buffer* self)
{
    GG_ASSERT(self != NULL);
    GG_INTERFACE(self)->Release(self);
}

//----------------------------------------------------------------------
const uint8_t*
GG_Buffer_GetData(const GG_Buffer* self)
{
    GG_ASSERT(self != NULL);
    return GG_INTERFACE(self)->GetData(self);
}

//----------------------------------------------------------------------
uint8_t*
GG_Buffer_UseData(GG_Buffer* self)
{
    GG_ASSERT(self != NULL);
    return GG_INTERFACE(self)->UseData(self);
}

//----------------------------------------------------------------------
size_t
GG_Buffer_GetDataSize(const GG_Buffer* self)
{
    GG_ASSERT(self != NULL);
    return GG_INTERFACE(self)->GetDataSize(self);
}

//----------------------------------------------------------------------
void
GG_StaticBuffer_Init(GG_StaticBuffer* self, const uint8_t* data, size_t data_size)
{
    GG_ASSERT(self != NULL);
    GG_ASSERT(data != NULL);

    GG_SET_INTERFACE(self, GG_StaticBuffer, GG_Buffer);
    self->data       = data;
    self->data_size  = data_size;
}

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_DynamicBuffer {
    GG_IMPLEMENTS(GG_Buffer);

    unsigned int reference_counter;
    bool         buffer_is_local;
    uint8_t*     buffer;
    size_t       buffer_size;
    size_t       data_size;
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_DATA_BUFFER_EXTRA_GROW_SPACE     256
#define GG_DATA_BUFFER_TRY_DOUBLE_THRESHOLD 4096

//----------------------------------------------------------------------
GG_Buffer*
GG_DynamicBuffer_AsBuffer(GG_DynamicBuffer* self)
{
    return GG_CAST(self, GG_Buffer);
}

//----------------------------------------------------------------------
GG_DynamicBuffer*
GG_DynamicBuffer_Retain(GG_DynamicBuffer* self)
{
    ++self->reference_counter;
    return self;
}

//----------------------------------------------------------------------
static GG_Buffer*
GG_DynamicBuffer_Retain_(GG_Buffer* _self)
{
    GG_DynamicBuffer* self = GG_SELF(GG_DynamicBuffer, GG_Buffer);
    return GG_CAST(GG_DynamicBuffer_Retain(self), GG_Buffer);
}

//----------------------------------------------------------------------
static void
GG_DynamicBuffer_Destroy(GG_DynamicBuffer* self)
{
    if (!self) {
        return;
    }

    /* free the buffer */
    if (self->buffer_is_local) GG_FreeMemory((void*)self->buffer);

    /* free the object */
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
void
GG_DynamicBuffer_Release(GG_DynamicBuffer* self)
{
    if (--self->reference_counter == 0) {
        GG_DynamicBuffer_Destroy(self);
    }
}

//----------------------------------------------------------------------
static void
GG_DynamicBuffer_Release_(GG_Buffer* _self)
{
    GG_DynamicBuffer* self = GG_SELF(GG_DynamicBuffer, GG_Buffer);
    GG_DynamicBuffer_Release(self);
}

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_Clone(const GG_DynamicBuffer* self, GG_DynamicBuffer** clone)
{
    /* create a clone with a buffer of the same size */
    GG_CHECK(GG_DynamicBuffer_Create(self->data_size, clone));

    /* copy the data */
    GG_CHECK(GG_DynamicBuffer_SetData(*clone,
                                      self->buffer,
                                      self->data_size));
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_DynamicBuffer_ReallocateBuffer(GG_DynamicBuffer* self, size_t size)
{
    uint8_t* new_buffer;

    /* check that the existing data fits */
    if (self->data_size > size) return GG_ERROR_INVALID_PARAMETERS;

    /* allocate a new buffer */
    if (size) {
        new_buffer = (uint8_t*)GG_AllocateMemory(size);
        if (new_buffer == NULL) return GG_ERROR_OUT_OF_MEMORY;

        /* copy the contents of the previous buffer, if any */
        if (self->buffer && self->data_size) {
            memcpy(new_buffer, self->buffer, self->data_size);
        }
    } else {
        new_buffer = NULL;
    }

    /* destroy the previous buffer */
    GG_FreeMemory((void*)self->buffer);

    /* use the new buffer */
    self->buffer = new_buffer;
    self->buffer_size = size;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_SetBuffer(GG_DynamicBuffer* self,
                           uint8_t*          buffer,
                           size_t            buffer_size)
{
    if (self->buffer_is_local) {
        /* destroy the local buffer */
        GG_FreeMemory((void*)self->buffer);
    }

    /* we're now using an external buffer */
    self->buffer_is_local = false;
    self->buffer = buffer;
    self->buffer_size = buffer_size;
    self->data_size = 0;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_SetBufferSize(GG_DynamicBuffer* self,
                               size_t            buffer_size)
{
    if (self->buffer_is_local) {
        return GG_DynamicBuffer_ReallocateBuffer(self, buffer_size);
    } else {
        /* cannot change an external buffer */
        return GG_ERROR_NOT_SUPPORTED;
    }
}

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_Reserve(GG_DynamicBuffer* self, size_t size)
{
    if (self->buffer_size >= size) return GG_SUCCESS;

    // if the buffer is small enough, try doubling the size
    size_t new_size = size;
    if (self->buffer_size < GG_DATA_BUFFER_TRY_DOUBLE_THRESHOLD) {
        new_size = self->buffer_size * 2;
        if (new_size < size) {
            // still too small, allocate the requested size plus some extra space for future growth
            new_size = size + GG_DATA_BUFFER_EXTRA_GROW_SPACE;
            if (new_size < size) {
                // overflow, just allocate the exact requested size without the extra space
                new_size = size;
            }
        }
    }
    return GG_DynamicBuffer_SetBufferSize(self, new_size);
}

//----------------------------------------------------------------------
size_t
GG_DynamicBuffer_GetBufferSize(const GG_DynamicBuffer* self)
{
    return self->buffer_size;
}

//----------------------------------------------------------------------
const uint8_t*
GG_DynamicBuffer_GetData(const GG_DynamicBuffer* self)
{
    return self->buffer;
}

//----------------------------------------------------------------------
static const uint8_t*
GG_DynamicBuffer_GetData_(const GG_Buffer* _self)
{
    const GG_DynamicBuffer* self = GG_SELF(GG_DynamicBuffer, GG_Buffer);
    return GG_DynamicBuffer_GetData(self);
}

//----------------------------------------------------------------------
uint8_t*
GG_DynamicBuffer_UseData(GG_DynamicBuffer* self)
{
    return self->buffer;
}

//----------------------------------------------------------------------
static uint8_t*
GG_DynamicBuffer_UseData_(GG_Buffer* _self)
{
    GG_DynamicBuffer* self = GG_SELF(GG_DynamicBuffer, GG_Buffer);
    return GG_DynamicBuffer_UseData(self);
}

//----------------------------------------------------------------------
size_t
GG_DynamicBuffer_GetDataSize(const GG_DynamicBuffer* self)
{
    return self->data_size;
}

//----------------------------------------------------------------------
static size_t
GG_DynamicBuffer_GetDataSize_(const GG_Buffer* _self)
{
    const GG_DynamicBuffer* self = GG_SELF(GG_DynamicBuffer, GG_Buffer);
    return GG_DynamicBuffer_GetDataSize(self);
}

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_SetDataSize(GG_DynamicBuffer* self, size_t size)
{
    if (size > self->buffer_size) {
        /* the buffer is too small, we need to reallocate it */
        if (self->buffer_is_local) {
            GG_CHECK(GG_DynamicBuffer_ReallocateBuffer(self, size));
        } else {
            /* we cannot reallocate an external buffer */
            return GG_ERROR_NOT_SUPPORTED;
        }
    }
    self->data_size = size;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_SetData(GG_DynamicBuffer* self,
                         const uint8_t*    data,
                         size_t            data_size)
{
    if (data_size > self->buffer_size) {
        if (self->buffer_is_local) {
            GG_CHECK(GG_DynamicBuffer_ReallocateBuffer(self, data_size));
        } else {
            return GG_ERROR_OUT_OF_RESOURCES;
        }
    }
    memcpy(self->buffer, data, data_size);
    self->data_size = data_size;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
bool
GG_DynamicBuffer_Equals(const GG_DynamicBuffer* self,
                        const GG_Buffer*        other)
{
    /* true if both are NULL */
    if (self == NULL && other == NULL) return true;

    /* not true if one of them is NULL */
    if (self == NULL || other == NULL) return false;

    /* check that the sizes are the same */
    if (self->data_size != GG_Buffer_GetDataSize(other)) return false;

    /* now compare the data */
    return memcmp(self->buffer, GG_Buffer_GetData(other), self->data_size) == 0;
}

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_AppendData(GG_DynamicBuffer* self,
                            const uint8_t*    data,
                            size_t            data_size)
{
    size_t   new_data_size = self->data_size + data_size;

    /* reserve the space and copy the appended data */
    GG_CHECK(GG_DynamicBuffer_Reserve(self, new_data_size));
    memcpy(self->buffer + self->data_size, data, data_size);
    self->data_size = new_data_size;

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   function table
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_DynamicBuffer, GG_Buffer)
{
    GG_DynamicBuffer_Retain_,
    GG_DynamicBuffer_Release_,
    GG_DynamicBuffer_GetData_,
    GG_DynamicBuffer_UseData_,
    GG_DynamicBuffer_GetDataSize_
};

//----------------------------------------------------------------------
GG_Result
GG_DynamicBuffer_Create(size_t size, GG_DynamicBuffer** buffer)
{
    /* allocate the object */
    *buffer = GG_AllocateZeroMemory(sizeof(GG_DynamicBuffer));
    if (*buffer == NULL) return GG_ERROR_OUT_OF_MEMORY;

    /* construct the object */
    (*buffer)->reference_counter = 1;
    (*buffer)->buffer_is_local   = true;

    /* allocate the buffer */
    if (size) {
        (*buffer)->buffer_size = size;
        (*buffer)->buffer = GG_AllocateMemory(size);
        if ((*buffer)->buffer == NULL) {
            GG_FreeMemory((void*)(*buffer));
            return GG_ERROR_OUT_OF_MEMORY;
        }
    }

    /* setup the interfaces */
    GG_SET_INTERFACE(*buffer, GG_DynamicBuffer, GG_Buffer);

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_SubBuffer {
    GG_IMPLEMENTS(GG_Buffer);

    unsigned int reference_counter;
    GG_Buffer*   data;
    size_t       data_offset;
    size_t       data_size;
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Buffer*
GG_SubBuffer_Retain(GG_Buffer* _self)
{
    GG_SubBuffer* self = (GG_SubBuffer*)GG_SELF(GG_SubBuffer, GG_Buffer);
    ++self->reference_counter;
    return _self;
}

//----------------------------------------------------------------------
static void
GG_SubBuffer_Release(GG_Buffer* _self)
{
    GG_SubBuffer* self = (GG_SubBuffer*)GG_SELF(GG_SubBuffer, GG_Buffer);
    if (--self->reference_counter == 0) {
        GG_Buffer_Release(self->data);
        GG_FreeMemory(self);
    }
}

//----------------------------------------------------------------------
static const uint8_t*
GG_SubBuffer_GetData(const GG_Buffer* _self)
{
    GG_SubBuffer* self = (GG_SubBuffer*)GG_SELF(GG_SubBuffer, GG_Buffer);
    return GG_Buffer_GetData(self->data) + self->data_offset;
}

//----------------------------------------------------------------------
static uint8_t*
GG_SubBuffer_UseData(GG_Buffer* _self)
{
    GG_SubBuffer* self = (GG_SubBuffer*)GG_SELF(GG_SubBuffer, GG_Buffer);
    return GG_Buffer_UseData(self->data) + self->data_offset;
}

//----------------------------------------------------------------------
static size_t
GG_SubBuffer_GetDataSize(const GG_Buffer* _self)
{
    GG_SubBuffer* self = (GG_SubBuffer*)GG_SELF(GG_SubBuffer, GG_Buffer);
    return self->data_size;
}

/*----------------------------------------------------------------------
|   function table
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_SubBuffer, GG_Buffer)
{
    GG_SubBuffer_Retain,
    GG_SubBuffer_Release,
    GG_SubBuffer_GetData,
    GG_SubBuffer_UseData,
    GG_SubBuffer_GetDataSize
};

//----------------------------------------------------------------------
GG_Result
GG_SubBuffer_Create(GG_Buffer* data, size_t offset, size_t size, GG_Buffer** buffer)
{
    GG_ASSERT(data);

    // check the range
    *buffer = NULL;
    if (offset + size > GG_Buffer_GetDataSize(data)) {
        return GG_ERROR_OUT_OF_RANGE;
    }

    // allocate a new object
    GG_SubBuffer* self = (GG_SubBuffer*)GG_AllocateZeroMemory(sizeof(GG_SubBuffer));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    self->reference_counter = 1;
    self->data              = GG_Buffer_Retain(data);
    self->data_offset       = offset;
    self->data_size         = size;

    // setup the interface
    GG_SET_INTERFACE(self, GG_SubBuffer, GG_Buffer);

    // return the object
    *buffer = GG_CAST(self, GG_Buffer);

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/
void
GG_BufferSource_GetData(const GG_BufferSource* self, void* buffer)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->GetData(self, buffer);
}

//----------------------------------------------------------------------
size_t
GG_BufferSource_GetDataSize(const GG_BufferSource* self)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->GetDataSize(self);
}

//----------------------------------------------------------------------
static size_t
GG_StaticBufferSource_GetDataSize(const GG_BufferSource* _self)
{
    GG_StaticBufferSource* self = GG_SELF(GG_StaticBufferSource, GG_BufferSource);

    return self->data_size;
}

//----------------------------------------------------------------------
static void
GG_StaticBufferSource_GetData(const GG_BufferSource* _self, void* data)
{
    GG_StaticBufferSource* self = GG_SELF(GG_StaticBufferSource, GG_BufferSource);

    if (self->data && self->data_size) {
        memcpy(data, self->data, self->data_size);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_StaticBufferSource, GG_BufferSource)
{
    .GetDataSize = GG_StaticBufferSource_GetDataSize,
    .GetData     = GG_StaticBufferSource_GetData
};

//----------------------------------------------------------------------
void
GG_StaticBufferSource_Init(GG_StaticBufferSource* self, const uint8_t* data, size_t data_size)
{
    GG_ASSERT(self != NULL);

    GG_SET_INTERFACE(self, GG_StaticBufferSource, GG_BufferSource);
    self->data       = data;
    self->data_size  = data_size;
}
