/**
 * @file
 * @brief Data buffers
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
#include <stdbool.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Buffers Data Buffers
//! General purpose data buffers
//! @{

//----------------------------------------------------------------------
//! Interface implemented by objects that represent data
//! buffers that can be accessed via a direct pointer.
//!
//! @interface GG_Buffer
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_Buffer) {
    /**
     * Retain a reference to this object.
     *
     * @param self The object on which this method is invoked.
     * @return The retained object (which may or may not be the same pointer as the
     * one on which this method is called).
     */
    GG_Buffer* (*Retain)(GG_Buffer* self);

    /**
     * Release a reference to this object.
     *
     * @param self The object on which this method is invoked.
     */
    void (*Release)(GG_Buffer* self);

    /**
     * Obtain a read-only pointer to the data represented by a buffer object.
     *
     * @param self The object on which this method is invoked.
     * @return Const pointer to the data.
     */
    const uint8_t* (*GetData)(const GG_Buffer* self);

    /**
     * Obtain a read-write pointer to the data represented by a buffer object.
     *
     * @param self The object on which this method is invoked.
     * @return Pointer to the data, or NULL if the object's data is read-only.
     */
    uint8_t* (*UseData)(GG_Buffer* self);

    /**
     * Get the size of the data represented by a buffer object.
     *
     * @param self The object on which this method is invoked.
     * @return The size of the data.
     */
    size_t (*GetDataSize)(const GG_Buffer* self);
};

//! @var GG_Buffer::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_BufferInterface
//! Virtual function table for the GG_Buffer interface

//! @relates GG_Buffer
//! @copydoc GG_BufferInterface::Retain
GG_Buffer* GG_Buffer_Retain(GG_Buffer* self);

//! @relates GG_Buffer
//! @copydoc GG_BufferInterface::Release
void GG_Buffer_Release(GG_Buffer* self);

//! @relates GG_Buffer
//! @copydoc GG_BufferInterface::GetData
const uint8_t* GG_Buffer_GetData(const GG_Buffer* self);

//! @relates GG_Buffer
//! @copydoc GG_BufferInterface::UseData
uint8_t* GG_Buffer_UseData(GG_Buffer* self);

//! @relates GG_Buffer
//! @copydoc GG_BufferInterface::GetDataSize
size_t GG_Buffer_GetDataSize(const GG_Buffer* self);

//---------------------------------------------------------------------
//! @class GG_StaticBuffer
//! @implements GG_Buffer
//
//! Class that implements the GG_Buffer interface, providing a read-only
//! view of a static data buffer.
//---------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_Buffer); //!< GG_Buffer interface vtable.

    const uint8_t* data;      //!< Pointer to the read-only static data.
    size_t         data_size; //!< Size of the data.
} GG_StaticBuffer;

//! Obtain the GG_Buffer interface of a GG_StaticBuffer object.
//! @relates GG_StaticBuffer
#define GG_StaticBuffer_AsBuffer(buffer) GG_CAST(buffer, GG_Buffer)

//! Initialize a GG_StaticBuffer object.
//! @relates GG_StaticBuffer
void GG_StaticBuffer_Init(GG_StaticBuffer* self, const uint8_t* data, size_t data_size);

//---------------------------------------------------------------------
//! @class GG_DynamicBuffer
//! @implements GG_Buffer
//
//! Class that implements the GG_Buffer interface, with a dynamic heap-allocated
//! buffer storage, which can grow, or a user-supplied external buffer, which cannot
//! grow.
//---------------------------------------------------------------------
typedef struct GG_DynamicBuffer GG_DynamicBuffer;

/**
 * Create a new GG_DynamicBuffer object.
 *
 * @relates GG_DynamicBuffer
 * @param size Size to pre-reserve for the buffer (may be 0, it can grow later).
 * @param buffer Pointer to where the new object instance will be returned.
 * @return #GG_SUCCESS if the object could be created, or an error code.
 */
GG_Result GG_DynamicBuffer_Create(size_t size, GG_DynamicBuffer** buffer);

/**
 * Obtain the GG_Buffer interface for the object.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @return The GG_Buffer interface for the object.
 */
GG_Buffer* GG_DynamicBuffer_AsBuffer(GG_DynamicBuffer* self);

/**
 * @relates GG_DynamicBuffer
 * @see GG_BufferInterface::Retain
 */
GG_DynamicBuffer* GG_DynamicBuffer_Retain(GG_DynamicBuffer* self);

/**
 * @relates GG_DynamicBuffer
 * @see GG_BufferInterface::Release
 */
void GG_DynamicBuffer_Release(GG_DynamicBuffer* self);

/**
 * @relates GG_DynamicBuffer
 * @see GG_BufferInterface::GetData
 */
const uint8_t* GG_DynamicBuffer_GetData(const GG_DynamicBuffer* self);

/**
 * @relates GG_DynamicBuffer
 * @see GG_BufferInterface::UseData
 */
uint8_t* GG_DynamicBuffer_UseData(GG_DynamicBuffer* self);

/**
 * @relates GG_DynamicBuffer
 * @see GG_BufferInterface::GetDataSize
 */
size_t GG_DynamicBuffer_GetDataSize(const GG_DynamicBuffer* self);

/**
 * Clone the object.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param clone Pointer to where the cloned object will be returned.
 * @return #GG_SUCCESS if the object could be cloned, or an error code.
 */
GG_Result GG_DynamicBuffer_Clone(const GG_DynamicBuffer* self, GG_DynamicBuffer** clone);

/**
 * Configure the object to use an external buffer.
 * NOTE: after calling this method, the buffer can no longer grow.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param buffer Buffer to use for data storage.
 * @param buffer_size Size of the buffer.
 * @return #GG_SUCCESS if the object could be configured, or an error code.
 */
GG_Result  GG_DynamicBuffer_SetBuffer(GG_DynamicBuffer* self, uint8_t* buffer, size_t buffer_size);

/**
 * Update the size of the buffer used for data storage.
 * NOTE: this does not change the data size, it only changes the buffer reserved for
 * future growth of the data.
 * The buffer size cannot be less than the current data size of the object.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param buffer_size Size of the buffer.
 * @return #GG_SUCCESS if the object could be updated, or an error code.
 */
GG_Result  GG_DynamicBuffer_SetBufferSize(GG_DynamicBuffer* self, size_t buffer_size);

/**
 * Get the size of the buffer used by this object.
 * NOTE: the buffer size is not the data size. @see GG_DynamicBuffer_GetDataSize
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @return The size of the buffer.
 */
size_t GG_DynamicBuffer_GetBufferSize(const GG_DynamicBuffer* self);

/**
 * Reserve a minimum size the size for the buffer used for data storage.
 * NOTE: this does not shrink the current buffer if it is already large enough.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param buffer_size Minimum size of the buffer to reserve.
 * @return #GG_SUCCESS if the buffer size could be reserved, or an error code.
 */
GG_Result GG_DynamicBuffer_Reserve(GG_DynamicBuffer* self, size_t buffer_size);

/**
 * Sets the data.
 * The data is copied to the object's buffer, and the data size is updated.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param data The data to copy.
 * @param data_size The size of the data.
 * @return #GG_SUCCESS if the data could be copied, or an error code.
 */
GG_Result  GG_DynamicBuffer_SetData(GG_DynamicBuffer* self, const uint8_t* data, size_t data_size);

/**
 * Sets/updates the data size.
 * This updates the size of the data (typically called after updating the data
 * directly in the buffer obtained from GG_DynamicBuffer_UseData, to reflect the new size of the data)
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param size The size to set.
 * @return #GG_SUCCESS if the size could be updated, or an error code.
 */
GG_Result  GG_DynamicBuffer_SetDataSize(GG_DynamicBuffer* self, size_t size);

/**
 * Compare the contents of two data buffers.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param other The data buffer object with which to compare.
 * @return true if the data are the same, false if the data are different.
 */
bool GG_DynamicBuffer_Equals(const GG_DynamicBuffer* self, const GG_Buffer* other);

/**
 * Append data at the end of an object's existing data.
 * NOTE: this will automatically try to grow the buffer if needed.
 *
 * @relates GG_DynamicBuffer
 * @param self The object on which this method is invoked.
 * @param data The data to append.
 * @param data_size The size of the data to append.
 * @return #GG_SUCCESS if the data could be appended, or an error code.
 */
GG_Result GG_DynamicBuffer_AppendData(GG_DynamicBuffer* self, const uint8_t* data, size_t data_size);

//---------------------------------------------------------------------
//! @class GG_SubBuffer
//! @implements GG_Buffer
//
//! Class that implements the GG_Buffer interface, exposing a bytes range of another buffer.
//---------------------------------------------------------------------
typedef struct GG_SubBuffer GG_SubBuffer;

/**
 * Create a new GG_SubBuffer object.
 *
 * @relates GG_SubBuffer
 * @param data Buffer source data.
 * @param offset Offset of the byte range within the source data.
 * @param size Size of the byte range within the source data.
 * @param buffer Pointer to where the new object instance will be returned.
 * @return #GG_SUCCESS if the object could be created, or an error code.
 */
GG_Result GG_SubBuffer_Create(GG_Buffer* data, size_t offset, size_t size, GG_Buffer** buffer);

//----------------------------------------------------------------------
//! Interface implemented by objects that represent data
//! buffers that can write their data into another buffer
//!
//! @interface GG_BufferSource
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_BufferSource) {
    /**
     * Get the buffer data.
     *
     * @param self The object on which this method is invoked.
     * @param data The buffer to which to write the data.
     */
    void (*GetData)(const GG_BufferSource* self, void* data);

    /**
     * Get the size of the data represented by the object.
     *
     * @param self The object on which this method is invoked.
     * @return The size of the data.
     */
    size_t (*GetDataSize)(const GG_BufferSource* self);
};

//! @var GG_BufferSource::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_BufferSourceInterface
//! Virtual function table for the GG_BufferSource interface

//! @relates GG_BufferSource
//! @copydoc GG_BufferSourceInterface::Retain
void GG_BufferSource_GetData(const GG_BufferSource* self, void* data);

//! @relates GG_BufferSource
//! @copydoc GG_BufferSourceInterface::GetDataSize
size_t GG_BufferSource_GetDataSize(const GG_BufferSource* self);

//---------------------------------------------------------------------
//! @class GG_StaticBufferSource
//! @implements GG_BufferSource
//
//! Class that implements the GG_BufferSource interface, representing
//! a static data buffer.
//---------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_BufferSource); //!< GG_BufferSource interface vtable.

    const uint8_t* data;            //!< Pointer to the read-only static data.
    size_t         data_size;       //!< Size of the data.
} GG_StaticBufferSource;

//! Obtain the GG_BufferSource interface of a GG_StaticBufferSource object.
//! @relates GG_StaticBufferSource
#define GG_StaticBufferSource_AsBufferSource(buffer) GG_CAST(buffer, GG_BufferSource)

//! Initialize a GG_StaticBufferSource object.
//! @relates GG_StaticBufferSource
void GG_StaticBufferSource_Init(GG_StaticBufferSource* self, const uint8_t* data, size_t data_size);

//! @}

#if defined(__cplusplus)
}
#endif
