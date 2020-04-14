/**
 * @file
 * @brief Bitstream reader/writer
 *
 * @copyright
 * Copyright 2017-2019 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-03-08
*/

#pragma once

/*----------------------------------------------------------------------
|    includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"

//! @addtogroup Formats
//! Bitstream reading & writing
//! @{

/*----------------------------------------------------------------------
|       types helpers
+---------------------------------------------------------------------*/
#define GG_BITSTREAM_WORD_BITS  32
#define GG_BITSTREAM_WORD_BYTES 4

/*----------------------------------------------------------------------
|    types
+---------------------------------------------------------------------*/

/**
 * Bit Input Stream (stream of bits stored in a byte buffer)
 *
 * This class supports reading 1 to 32 bits at a time from a byte buffer.
 *
 */
typedef struct GG_BitInputStream
{
    const uint8_t*   bytes;         // Data from which bits are read
    size_t           byte_size;     // Data size
    size_t           byte_position; // Byte read position
    uint32_t         cache;         // Cached bits
    size_t           bits_cached;   // Number of bits in the cache
} GG_BitInputStream;

/**
 * Bit Output Stream (stream of bits stored in a byte buffer)
 *
 * This class supports writing 1 to 32 bits at a time to a byte buffer.
 *
 */
typedef struct GG_BitOutputStream
{
    uint8_t*         bytes;         // Buffer in which bits are written
    size_t           byte_size;     // Buffer size
    size_t           byte_position; // Byte write position
    uint32_t         cache;         // Cached bits
    size_t           bits_cached;   // Number of bits in the cache
} GG_BitOutputStream;

/*----------------------------------------------------------------------
|    prototypes
+---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Initialize a bitstream for reading.
 *
 * @param self The object on which the method is invoked.
 */
void GG_BitInputStream_Init(GG_BitInputStream* self, const uint8_t* data, size_t data_size);

/**
 * Reset a bitstream.
 * This is equivalent to calling GG_BitInputStream_Init again with the same parameters
 * as when the bitstream was initialized.
 *
 * @param self The object on which the method is invoked
 */
void GG_BitInputStream_Reset(GG_BitInputStream* self);

/**
 * Align the reading position to the nearest byte boundary.
 *
 * @param self The object on which the method is invoked.
 */
void GG_BitInputStream_ByteAlign(GG_BitInputStream* self);

/**
 * Get the current bit position of a bitstream.
 *
 * @param self The object on which the method is invoked.
 *
 * @return The current bit position.
 */
size_t GG_BitInputStream_GetPosition(GG_BitInputStream* self);

/**
 * Read bits from a bitstream.
 *
 * @param self The object on which the method is invoked.
 * @param bit_count The number of bits to read (between 1 and 32).
 *
 * @return The bits read from the buffer.
 */
uint32_t GG_BitInputStream_Read(GG_BitInputStream* self, size_t bit_count);

/**
 * Peek at bits from a bitstream.
 * This is the same as GG_BitInputStream_Read but without advancing the bit position.
 *
 * @param self The object on which the method is invoked.
 * @param bit_count The number of bits to read (between 1 and 32).
 *
 * @return The bits read from the buffer.
 */
uint32_t GG_BitInputStream_Peek(GG_BitInputStream* self, size_t bit_count);

/**
 * Initialize a bitstream for writing.
 */
void GG_BitOutputStream_Init(GG_BitOutputStream* self, uint8_t* buffer, size_t buffer_size);

/**
 * Reset a bitstream.
 * This is equivalent to calling GG_BitOutputStream_Init again with the same parameters
 * as when the bitstream was initialized.
 *
 * @param self The object on which the method is invoked
 */
void GG_BitOutputStream_Reset(GG_BitOutputStream* self);

/**
 * Write bits to a bitstream.
 * NOTE: the bits may not be immediately stored in the buffer.
 *
 * @param self The object on which the method is invoked.
 * @param bits Integer value representing the bits to write.
 * @param bit_count The number of bits to write (between 1 and 32).
 */
void GG_BitOutputStream_Write(GG_BitOutputStream* self, uint32_t bits, size_t bit_count);

/**
 * Flush the bitstream to the underlying byte buffer.
 * NOTE: this may write zero-padding bits. More bits written after calling this method
 * may not be contiguous. This method is typically called when the caller is done writing
 * and wants to use the values in the underlying buffer.
 *
 * @param self The object on which the method is invoked
 */
void GG_BitOutputStream_Flush(GG_BitOutputStream* self);

/**
 * Get the current bit position of a bitstream.
 *
 * @param self The object on which the method is invoked.
 *
 * @return The current bit position.
 */
size_t GG_BitOutputStream_GetPosition(GG_BitOutputStream* self);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
