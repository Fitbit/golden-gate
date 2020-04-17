/**
 * @file
 * @brief General purpose ring buffer
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-02
*/

#pragma once

/*----------------------------------------------------------------------
|    includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"

//! @addtogroup Buffers
//! Ring Buffer
//! @{

/*----------------------------------------------------------------------
|    types
+---------------------------------------------------------------------*/

/**
 * Ring Buffer (a.k.a Circular Buffer)
 */
typedef struct {
    struct {
        uint8_t* start;
        uint8_t* end;
    }        data;
    size_t   size;
    uint8_t* in;
    uint8_t* out;
} GG_RingBuffer;

/*----------------------------------------------------------------------
|    prototypes
+---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Initialize a ring buffer object
 *
 * @param self The object on which this method is invoked.
 * @param buffer the memory buffer used as storage for the object. This
 *               buffer needs to stay allocated at least as long as the
 *               ring buffer object that is being initialized by this call.
 * @param buffer_size Size of the buffer.
 */
void GG_RingBuffer_Init(GG_RingBuffer* self, uint8_t* buffer, size_t buffer_size);

/**
 * Get the writable space in the buffer (how much can be written).
 *
 * @param self The object on which this method is invoked.
 * @return The number of bytes that can be written to the buffer.
 */
size_t GG_RingBuffer_GetSpace(GG_RingBuffer* self);

/**
 * Get the number of bytes that can be writen contiguously given the
 * current state of the buffer.
 *
 * @param self The object on which this method is invoked.
 * @return The contiguous space available in the buffer.
 */
size_t GG_RingBuffer_GetContiguousSpace(GG_RingBuffer* self);

/**
 * Get the amount of data available from the buffer (how much can be read).
 *
 * @param self The object on which this method is invoked.
 * @return The number of bytes that can be read from the buffer.
 */
size_t GG_RingBuffer_GetAvailable(GG_RingBuffer* self);

/**
 * Get the amount of data availble from the buffer in a single contiguous block.
 *
 * @param self The object on which this method is invoked.
 * @return The number of contiguous bytes available.
 */
size_t GG_RingBuffer_GetContiguousAvailable(GG_RingBuffer* self);

/**
 * Read one byte from the buffer.
 * NOTE: This method does not do any bounds checking, so it always returns
 * a value regardless of whether any data is available from the buffer or
 * not. So it should only be called when the caller knows that data is available.
 *
 * @param self The object on which this method is invoked.
 * @return The byte read from the buffer.
 */
uint8_t GG_RingBuffer_ReadByte(GG_RingBuffer* self);

/**
 * Read data from the buffer.
 *
 * @param self The object on which this method is invoked.
 * @param buffer The memory into which to read the data.
 * @param size The number of bytes to read.
 *
 * @return The number of bytes actually read (which may be less
 *         than the requested amount if the buffer doesn't have enough data).
 */
size_t GG_RingBuffer_Read(GG_RingBuffer* self,
                          uint8_t*       buffer,
                          size_t         size);

/**
 * Peek one byte from the buffer.
 * NOTE: this method does not perform any bounds checking. It always returns
 * a value regardless of whether there is enough data in the buffer or not. So
 * it should only be called when the bounds are otherwise known by the caller.
 *
 * @param self The object on which this method is invoked.
 * @param offset Offset from the first available byte.
 *
 * @return The byte at the specified offset.
 */
uint8_t GG_RingBuffer_PeekByte(GG_RingBuffer* self, size_t offset);

/**
 * Peek data from the buffer.
 *
 * @param self The object on which this method is invoked.
 * @param buffer The memory into which to store the peeked data.
 * @param offset Offset from the start of the available data to peek from.
 * @param size Number of bytes to peek at.
 *
 * @return The number of bytes actually peeked (which may be less
 *         than the requested amount if the buffer doesnt have enough data).
 */
size_t GG_RingBuffer_Peek(GG_RingBuffer* self,
                          uint8_t*       buffer,
                          size_t         offset,
                          size_t         size);

/**
 * Write data to the buffer.
 *
 * @param self The object on which this method is invoked.
 * @param buffer Data to write.
 * @param size Size of the data to write.
 *
 * @return The number of bytes actually written (which may be less
 *         than the requested amount if the buffer doesn't have enough space).
 */
size_t GG_RingBuffer_Write(GG_RingBuffer* self,
                           const uint8_t* buffer,
                           size_t         size);

/**
 * Move the ring's `in` pointer of the buffer.
 * This method may be useful if data is directly copied to the buffer
 * without calling GG_RingBuffer_Write.
 *
 * @param self The object on which this method is invoked.
 * @param offset The number of bytes by which to advance the `in` pointer.
 */
void GG_RingBuffer_MoveIn(GG_RingBuffer* self, size_t offset);

/**
 * Move the ring's `out` pointer of the buffer.
 * This method may be useful if data is directly copied out the buffer
 * without calling GG_RingBuffer_Read.
 *
 * @param self The object on which this method is invoked.
 * @param offset The number of bytes by which to advance the `out` pointer.
 */
void GG_RingBuffer_MoveOut(GG_RingBuffer* self, size_t offset);

/**
 * Reset the buffer to its initial empty state.
 *
 * @param self The object on which this method is invoked.
 */
void GG_RingBuffer_Reset(GG_RingBuffer* self);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
