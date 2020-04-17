/**
 * @file
 * @brief Bitstream reader/writer
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-03-08
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_common.h"
#include "xp/common/gg_bitstream.h"

/*----------------------------------------------------------------------
|       macros
+---------------------------------------------------------------------*/
#define GG_BITSTREAM_BIT_MASK(_n) ((_n) ? (0xFFFFFFFF >> (GG_BITSTREAM_WORD_BITS - (_n))) : 0)

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_BitInputStream_Init(GG_BitInputStream* self, const uint8_t* data, size_t data_size)
{
    self->bytes         = data;
    self->byte_size     = data_size;
    self->byte_position = 0;
    self->cache         = 0;
    self->bits_cached   = 0;
}

//----------------------------------------------------------------------
void
GG_BitInputStream_Reset(GG_BitInputStream* self)
{
    self->byte_position = 0;
    self->cache         = 0;
    self->bits_cached   = 0;
}

//----------------------------------------------------------------------
void
GG_BitInputStream_ByteAlign(GG_BitInputStream* self)
{
    size_t bits_to_flush = self->bits_cached & 7;
    if (bits_to_flush) {
        GG_BitInputStream_Read(self, bits_to_flush);
    }
}

//----------------------------------------------------------------------
size_t
GG_BitInputStream_GetPosition(GG_BitInputStream* self)
{
    return (8 * self->byte_position) - self->bits_cached;
}

//----------------------------------------------------------------------
static uint32_t
GG_BitInputStream_ReadCache(const GG_BitInputStream* self, size_t* end_position)
{
    size_t position = self->byte_position;
    uint32_t result = 0;
    for (size_t i = 0; i < GG_BITSTREAM_WORD_BYTES; i++) {
        // don't read past the buffer
        uint8_t value = (position < self->byte_size) ? self->bytes[position] : 0;
        ++position;

        // shift the value in
        result = (result << 8) | value;
    }

    if (end_position) {
        *end_position = position;
    }

    return result;
}

//----------------------------------------------------------------------
uint32_t
GG_BitInputStream_Read(GG_BitInputStream* self, size_t bit_count)
{
    GG_ASSERT(bit_count <= GG_BITSTREAM_WORD_BITS);

    if (self->bits_cached >= bit_count) {
        // we have enough bits in the cache to satisfy the request
        self->bits_cached -= bit_count;
        return (self->cache >> self->bits_cached) & GG_BITSTREAM_BIT_MASK(bit_count);
    } else {
        // not enough bits in the cache, read the next word
        uint32_t word = GG_BitInputStream_ReadCache(self, &self->byte_position);

        // combine the new word and the cache, and update the state
        uint32_t cache = self->cache & GG_BITSTREAM_BIT_MASK(self->bits_cached);
        bit_count -= self->bits_cached;
        self->bits_cached = GG_BITSTREAM_WORD_BITS - bit_count;
        uint32_t result = (word >> self->bits_cached) |
                          (bit_count == GG_BITSTREAM_WORD_BITS ? 0 : (cache << bit_count));
        self->cache = word;
        return result;
    }
}

//----------------------------------------------------------------------
uint32_t
GG_BitInputStream_Peek(GG_BitInputStream* self, size_t bit_count)
{
    GG_ASSERT(bit_count <= GG_BITSTREAM_WORD_BITS);

    if (self->bits_cached >= bit_count) {
        // we have enough bits in the cache to satisfy the request
        return (self->cache >> (self->bits_cached - bit_count)) & GG_BITSTREAM_BIT_MASK(bit_count);
    } else {
        // not enough bits in the cache, read the next word */
        uint32_t word = GG_BitInputStream_ReadCache(self, NULL);

        // combine the new word and the cache, and update the state
        uint32_t cache = self->cache & GG_BITSTREAM_BIT_MASK(self->bits_cached);
        bit_count -= self->bits_cached;
        return (word >> (GG_BITSTREAM_WORD_BITS - bit_count)) |
               (bit_count == GG_BITSTREAM_WORD_BITS ? 0 : (cache << bit_count));
    }
}

//----------------------------------------------------------------------
void
GG_BitOutputStream_Init(GG_BitOutputStream* self, uint8_t* buffer, size_t buffer_size)
{
    self->bytes         = buffer;
    self->byte_size     = buffer_size;
    self->byte_position = 0;
    self->cache         = 0;
    self->bits_cached   = 0;
}

//----------------------------------------------------------------------
void
GG_BitOutputStream_Reset(GG_BitOutputStream* self)
{
    self->byte_position = 0;
    self->cache         = 0;
    self->bits_cached   = 0;
}

//----------------------------------------------------------------------
void
GG_BitOutputStream_Write(GG_BitOutputStream* self, uint32_t bits, size_t bit_count)
{
    GG_ASSERT(bit_count <= GG_BITSTREAM_WORD_BITS);

    bits &= GG_BITSTREAM_BIT_MASK(bit_count);
    if (self->bits_cached + bit_count <= GG_BITSTREAM_WORD_BITS) {
        // we can store the bits in the cache
        self->cache = (bit_count == GG_BITSTREAM_WORD_BITS ? 0 : (self->cache << bit_count)) | bits;
        self->bits_cached += bit_count;
    } else {
        // combine the cache and part of the bits
        size_t partial_bit_count = GG_BITSTREAM_WORD_BITS - self->bits_cached;
        uint32_t word = partial_bit_count ?
                        (self->cache << partial_bit_count) | (bits >> (bit_count - partial_bit_count)) :
                        self->cache;

        // write the combined bits to the buffer
        for (size_t i = 0; i < GG_BITSTREAM_WORD_BYTES; i++) {
            // don't write past the buffer
            if (self->byte_position < self->byte_size) {
                self->bytes[self->byte_position] = (uint8_t)(word >> (8 * (GG_BITSTREAM_WORD_BYTES - i - 1)));
            }
            ++self->byte_position;
        }

        // update the cache
        self->cache = bits & GG_BITSTREAM_BIT_MASK(bit_count - partial_bit_count);
        self->bits_cached = bit_count - partial_bit_count;
    }
}

//----------------------------------------------------------------------
size_t
GG_BitOutputStream_GetPosition(GG_BitOutputStream* self)
{
    return (8 * self->byte_position) + self->bits_cached;
}

//----------------------------------------------------------------------
void
GG_BitOutputStream_Flush(GG_BitOutputStream* self)
{
    // write zeros to flush the cache
    if (self->bits_cached) {
        size_t position = self->byte_position;
        GG_BitOutputStream_Write(self, 0, (size_t)(1 + GG_BITSTREAM_WORD_BITS - self->bits_cached));

        // clear the cache and ajust the byte position
        self->byte_position = GG_MIN(self->byte_size, position + ((self->bits_cached + 7) / 8));
        self->bits_cached = 0;
    }
}
