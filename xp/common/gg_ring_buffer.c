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

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_common.h"
#include "xp/common/gg_ring_buffer.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
void
GG_RingBuffer_Init(GG_RingBuffer* self, uint8_t* buffer, size_t buffer_size)
{
    self->size           = buffer_size;
    self->data.start     = buffer;
    self->data.end       = self->data.start + buffer_size;
    self->in = self->out = self->data.start;
}

/*--------------------------------------------------------------------*/
size_t
GG_RingBuffer_GetContiguousSpace(GG_RingBuffer* self)
{
    return (size_t)
        ((self->in < self->out) ?
         (self->out - self->in - 1) :
         ((self->out == self->data.start) ?
          (self->data.end - self->in - 1) :
          (self->data.end - self->in)));
}

/*--------------------------------------------------------------------*/
size_t
GG_RingBuffer_GetSpace(GG_RingBuffer* self)
{
    return (size_t)
        ((self->in < self->out) ?
         (self->out - self->in - 1) :
         (self->data.end - self->in + self->out - self->data.start - 1));
}

/*--------------------------------------------------------------------*/
size_t
GG_RingBuffer_Write(GG_RingBuffer* self,
                    const uint8_t* buffer,
                    size_t         size)
{
    // clamp to what can be written
    size_t can_write = GG_RingBuffer_GetSpace(self);
    if (size > can_write) {
        size = can_write;
    }

    // shortcut
    if (!size) return 0;

    if (self->in < self->out) {
        memcpy(self->in, buffer, size);
        self->in += size;
        if (self->in == self->data.end) {
            self->in = self->data.start;
        }
    } else {
        size_t chunk = (size_t)(self->data.end - self->in);
        if (chunk >= size) {
            chunk = size;
        }

        memcpy(self->in, buffer, chunk);
        self->in += chunk;
        if (self->in == self->data.end) {
            self->in = self->data.start;
        }
        if (chunk != size) {
            memcpy(self->in, buffer+chunk, size-chunk);
            self->in += size-chunk;
            if (self->in == self->data.end) {
                self->in = self->data.start;
            }
        }
    }

    return size;
}

/*--------------------------------------------------------------------*/
size_t
GG_RingBuffer_GetContiguousAvailable(GG_RingBuffer* self)
{
    return (size_t)
        ((self->out <= self->in) ?
         (self->in - self->out) :
         (self->data.end - self->out));
}

/*--------------------------------------------------------------------*/
size_t
GG_RingBuffer_GetAvailable(GG_RingBuffer* self)
{
    return (size_t)
        ((self->out <= self->in) ?
         (self->in - self->out) :
         (self->data.end - self->out + self->in - self->data.start));
}

/*--------------------------------------------------------------------*/
size_t
GG_RingBuffer_Read(GG_RingBuffer* self,
                   uint8_t*       buffer,
                   size_t         size)
{
    // clamp to what can be read
    size_t can_read = GG_RingBuffer_GetAvailable(self);
    if (size > can_read) {
        size = can_read;
    }

    // shortcut
    if (!size) return 0;

    if (self->in > self->out) {
        memcpy(buffer, self->out, size);
        self->out += size;
        if (self->out == self->data.end) {
            self->out = self->data.start;
        }
    } else {
        size_t chunk = (size_t)(self->data.end - self->out);
        if (chunk >= size) {
            chunk = size;
        }

        memcpy(buffer, self->out, chunk);
        self->out += chunk;
        if (self->out == self->data.end) {
            self->out = self->data.start;
        }
        if (chunk != size) {
            memcpy(buffer+chunk, self->out, size-chunk);
            self->out += size-chunk;
            if (self->out == self->data.end) {
                self->out = self->data.start;
            }
        }
    }

    return size;
}

/*--------------------------------------------------------------------*/
size_t
GG_RingBuffer_Peek(GG_RingBuffer* self,
                   uint8_t*       buffer,
                   size_t         offset,
                   size_t         size)
{
    // clamp to what can be read
    size_t can_read = GG_RingBuffer_GetAvailable(self);
    if (offset >= can_read) return 0;
    can_read -= offset;
    if (size > can_read) {
        size = can_read;
    }

    // shortcut
    if (!size) return 0;

    const uint8_t* out = self->out + offset;
    if (out >= self->data.end) {
        out -= (self->data.end - self->data.start);
    }
    if (self->in > out) {
        memcpy(buffer, out, size);
    } else {
        size_t chunk = (size_t)(self->data.end - out);
        if (chunk >= size) {
            chunk = size;
        }

        memcpy(buffer, out, chunk);
        if (chunk != size) {
            memcpy(buffer+chunk, self->data.start, size-chunk);
        }
    }

    return size;
}

/*--------------------------------------------------------------------*/
uint8_t
GG_RingBuffer_ReadByte(GG_RingBuffer* self)
{
    GG_ASSERT(self->in != self->out);

    uint8_t result = *self->out++;
    if (self->out == self->data.end) {
        self->out = self->data.start;
    }
    return result;
}

/*--------------------------------------------------------------------*/
uint8_t
GG_RingBuffer_PeekByte(GG_RingBuffer* self, size_t offset)
{
    const uint8_t* where = self->out + offset;
    if (where >= self->data.end) {
        where -= (self->data.end - self->data.start);
    }

    return *where;
}

/*--------------------------------------------------------------------*/
void
GG_RingBuffer_MoveIn(GG_RingBuffer* self, size_t offset)
{
    self->in += offset;
    if (self->in < self->data.start) {
        self->in += self->size;
    } else if (self->in >= self->data.end) {
        self->in -= self->size;
    }
}

/*--------------------------------------------------------------------*/
void
GG_RingBuffer_MoveOut(GG_RingBuffer* self, size_t offset)
{
    self->out += offset;
    if (self->out < self->data.start) {
        self->out += self->size;
    } else if (self->out >= self->data.end) {
        self->out -= self->size;
    }
}

/*--------------------------------------------------------------------*/
void
GG_RingBuffer_Reset(GG_RingBuffer* self)
{
    self->in = self->out = self->data.start;
}
