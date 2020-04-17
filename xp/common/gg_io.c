/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 *
 * @details
 *
 * Implementation of the I/O interfaces.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_io.h"
#include "gg_results.h"
#include "gg_port.h"
#include "gg_utils.h"
#include "gg_logging.h"
#include "gg_memory.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_INPUT_STREAM_LOAD_DEFAULT_READ_CHUNK 4096

/*----------------------------------------------------------------------
|   GG_DataSink thunks
+---------------------------------------------------------------------*/
GG_Result
GG_DataSink_PutData(GG_DataSink* self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_ASSERT(self);
    GG_ASSERT(data);
    return GG_INTERFACE(self)->PutData(self, data, metadata);
}

GG_Result
GG_DataSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->SetListener(self, listener);
}

/*----------------------------------------------------------------------
|   GG_DataSource thunks
+---------------------------------------------------------------------*/
GG_Result
GG_DataSource_SetDataSink(GG_DataSource* self, GG_DataSink* sink)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->SetDataSink(self, sink);
}

/*----------------------------------------------------------------------
|   GG_DataSinkListener thunks
+---------------------------------------------------------------------*/
void
GG_DataSinkListener_OnCanPut(GG_DataSinkListener* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnCanPut(self);
}

/*----------------------------------------------------------------------
|   GG_InputStream thunks
+---------------------------------------------------------------------*/
GG_InputStream*
GG_InputStream_Retain(GG_InputStream* self)
{
    return GG_INTERFACE(self)->Retain(self);
}

/*----------------------------------------------------------------------
|   functions and methods
+---------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
GG_Result
GG_BufferMetadata_Clone(const GG_BufferMetadata* self, GG_BufferMetadata** clone)
{
    // shortcut
    if (self == NULL || self->size == 0) {
        *clone = NULL;
        return GG_SUCCESS;
    }

    // allocate and copy the clone
    *clone = GG_AllocateZeroMemory(self->size);
    if (*clone == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }
    memcpy(*clone, self, self->size);

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
void
GG_InputStream_Release(GG_InputStream* self)
{
    GG_INTERFACE(self)->Release(self);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_Read(GG_InputStream* self,
                    void*           buffer,
                    size_t          bytes_to_read,
                    size_t*         bytes_read)
{
    return GG_INTERFACE(self)->Read(self, buffer, bytes_to_read, bytes_read);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_Seek(GG_InputStream* self, GG_Position offset)
{
    return GG_INTERFACE(self)->Seek(self, offset);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_Tell(GG_InputStream* self, GG_Position* offset)
{
    return GG_INTERFACE(self)->Tell(self, offset);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_GetSize(GG_InputStream* self, GG_Position* size)
{
    return GG_INTERFACE(self)->GetSize(self, size);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_GetAvailable(GG_InputStream* self, GG_Position* available)
{
    return GG_INTERFACE(self)->GetAvailable(self, available);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_ReadLine(GG_InputStream* self,
                        char*           buffer,
                        size_t          size,
                        size_t*         chars_read)
{
    GG_Result result;
    size_t    total = 0;

    /* check parameters */
    if (buffer == NULL || size < 1) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    /* read until EOF or newline */
    do {
        size_t bytes_read;
        result = GG_InputStream_Read(self, (uint8_t*)buffer, 1, &bytes_read);
        if (GG_SUCCEEDED(result) && bytes_read == 1) {
            if (*buffer == '\n') {
                *buffer = '\0';
                if (chars_read) *chars_read = total;
                return GG_SUCCESS;
            }  else if (*buffer == '\r') {
                continue;
            }
        } else {
            if (chars_read) *chars_read = total;
            if (result == GG_ERROR_EOS) {
                *buffer = '\0';
                if (total != 0) {
                    return GG_SUCCESS;
                } else {
                    return GG_ERROR_EOS;
                }
            } else {
                return result;
            }
        }
        total++;
        buffer++;
    } while (total < size-1);

    /* terminate the line */
    *buffer = '\0';

    /* return what we have */
    if (chars_read) *chars_read = total;
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_ReadLineString(GG_InputStream* self,
                              GG_String*      string,
                              size_t          max_length)
{
    GG_Result result;

    /* reset the string */
    GG_String_SetLength(string, 0);

    /* read until EOF or newline */
    do {
        size_t bytes_read;
        char   c;
        result = GG_InputStream_Read(self, (uint8_t*)&c, 1, &bytes_read);
        if (GG_SUCCEEDED(result) && bytes_read == 1) {
            if (c == '\n') {
                return GG_SUCCESS;
            }  else if (c == '\r') {
                continue;
            }
        } else {
            if (result == GG_ERROR_EOS) {
                return GG_String_IsEmpty(string)?GG_ERROR_EOS:GG_SUCCESS;
            } else {
                return result;
            }
        }
        GG_String_AppendChar(string, c);
    } while (GG_String_GetLength(string)<max_length);

    return GG_ERROR_NOT_ENOUGH_SPACE;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_ReadUI16(GG_InputStream* self, uint16_t* value)
{
    unsigned char buffer[2];

    /* read bytes from the stream */
    GG_Result result;
    result = GG_InputStream_ReadFully(self, (void*)buffer, 2);
    if (GG_FAILED(result)) {
        *value = 0;
        return result;
    }

    /* convert bytes to value */
    *value = GG_BytesToInt16Be(buffer);

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_ReadUI32(GG_InputStream* self, uint32_t* value)
{
    unsigned char buffer[4];

    /* read bytes from the stream */
    GG_Result result;
    result = GG_InputStream_ReadFully(self, (void*)buffer, 4);
    if (GG_FAILED(result)) {
        *value = 0;
        return result;
    }

    /* convert bytes to value */
    *value = GG_BytesToInt32Be(buffer);

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_ReadUI64(GG_InputStream* self, uint64_t* value)
{
    unsigned char buffer[8];

    /* read bytes from the stream */
    GG_Result result;
    result = GG_InputStream_ReadFully(self, (void*)buffer, 8);
    if (GG_FAILED(result)) {
        *value = 0;
        return result;
    }

    /* convert bytes to value */
    *value = GG_BytesToInt64Be(buffer);

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_ReadFully(GG_InputStream* self,
                         void*           buffer,
                         size_t          bytes_to_read)
{
    size_t    bytes_read;
    GG_Result result;

    while (bytes_to_read) {
        result = GG_InputStream_Read(self, buffer, bytes_to_read, &bytes_read);
        if (GG_FAILED(result)) return result;
        if (bytes_read == 0) return GG_ERROR_INTERNAL;
        bytes_to_read -= bytes_read;
        buffer         = ((uint8_t*)bytes_read + bytes_read);
    }

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_Skip(GG_InputStream* self, size_t count)
{
    GG_Position position;
    GG_Result   result;

    /* get the current location */
    result = GG_InputStream_Tell(self, &position);
    if (GG_FAILED(result)) return result;

    /* seek ahead */
    return GG_InputStream_Seek(self, position+count);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_InputStream_Load(GG_InputStream* self, size_t max_read, GG_DynamicBuffer** buffer)
{
    GG_Result   result;
    GG_Position total_bytes_read;
    GG_Position size = 0;

    /* create a buffer if none was given */
    if (*buffer == NULL) {
        result = GG_DynamicBuffer_Create(0, buffer);
        if (GG_FAILED(result)) return result;
    }

    /* reset the buffer */
    GG_DynamicBuffer_SetDataSize(*buffer, 0);

    /* try to get the stream size */
    if (GG_FAILED(GG_InputStream_GetSize(self, &size))) {
        size = max_read;
    }  else {
        if (max_read && max_read < size) size = max_read;
    }

    /* pre-allocate the buffer */
    if (size) {
        result = GG_DynamicBuffer_Reserve(*buffer, (size_t)size);
        if (GG_FAILED(result)) return result;
    }

    /* read the data from the file */
    total_bytes_read = 0;
    do {
        GG_Position available = 0;
        GG_Position bytes_to_read;
        size_t      bytes_read;
        uint8_t*    data;

        /* check if we know how much data is available */
        result = GG_InputStream_GetAvailable(self, &available);
        if (GG_SUCCEEDED(result) && available) {
            /* we know how much is available */
            bytes_to_read = available;
        } else {
            bytes_to_read = GG_INPUT_STREAM_LOAD_DEFAULT_READ_CHUNK;
        }

        /* make sure we don't read more than what was asked */
        if (size != 0 && total_bytes_read+bytes_to_read>size) {
            bytes_to_read = size-total_bytes_read;
        }

        /* stop if we've read everything */
        if (bytes_to_read == 0) break;

        /* allocate space in the buffer */
        result = GG_DynamicBuffer_Reserve(*buffer, (size_t)(total_bytes_read+bytes_to_read));
        if (GG_FAILED(result)) return result;

        /* read the data */
        data = GG_DynamicBuffer_UseData(*buffer)+total_bytes_read;
        result = GG_InputStream_Read(self, (void*)data, (size_t)bytes_to_read, &bytes_read);
        if (GG_SUCCEEDED(result) && bytes_read != 0) {
            total_bytes_read += bytes_read;
            GG_DynamicBuffer_SetDataSize(*buffer, (size_t)total_bytes_read);
        }
    } while (GG_SUCCEEDED(result) && (size==0 || total_bytes_read < size));

    if (result == GG_ERROR_EOS) {
        return GG_SUCCESS;
    } else {
        return result;
    }
}

/*--------------------------------------------------------------------*/
GG_OutputStream*
GG_OutputStream_Retain(GG_OutputStream* self)
{
    return GG_INTERFACE(self)->Retain(self);
}

/*--------------------------------------------------------------------*/
void
GG_OutputStream_Release(GG_OutputStream* self)
{
    GG_INTERFACE(self)->Release(self);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_OutputStream_Write(GG_OutputStream* self,
                      const void*      buffer,
                      size_t           bytes_to_write,
                      size_t*          bytes_written)
{
    return GG_INTERFACE(self)->Write(self, buffer, bytes_to_write, bytes_written);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_OutputStream_Seek(GG_OutputStream* self, GG_Position offset)
{
    return GG_INTERFACE(self)->Seek(self, offset);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_OutputStream_Tell(GG_OutputStream* self, GG_Position* offset)
{
    return GG_INTERFACE(self)->Tell(self, offset);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_OutputStream_Flush(GG_OutputStream* self)
{
    return GG_INTERFACE(self)->Flush(self);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_OutputStream_WriteFully(GG_OutputStream* self,
                           const void*      buffer,
                           size_t           bytes_to_write)
{
    /* shortcut */
    if (bytes_to_write == 0) return GG_SUCCESS;

    /* write until failure */
    while (bytes_to_write) {
        size_t bytes_written;
        GG_Result result = GG_OutputStream_Write(self, buffer, bytes_to_write, &bytes_written);
        if (GG_FAILED(result)) return result;
        if (bytes_written == 0) return GG_ERROR_INTERNAL;
        GG_ASSERT(bytes_written <= bytes_to_write);
        bytes_to_write -= bytes_written;
        buffer = (const uint8_t*)buffer + bytes_written;
    }

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_OutputStream_WriteString(GG_OutputStream* self, const char* string)
{
    /* shortcut */
    if (string == NULL) {
        return GG_SUCCESS;
    }
    size_t string_length = strlen(string);
    if (string_length == 0) {
        return GG_SUCCESS;
    }

    /* write the string */
    return GG_OutputStream_WriteFully(self, (const void*)string, string_length);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_OutputStream_WriteLine(GG_OutputStream* self, const char* string)
{
    GG_Result result;
    result = GG_OutputStream_WriteString(self, string);
    if (GG_FAILED(result)) return result;
    result = GG_OutputStream_WriteFully(self, (const void*)"\r\n", 2);
    if (GG_FAILED(result)) return result;

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_MemoryStream
+---------------------------------------------------------------------*/
struct GG_MemoryStream {
    /* interfaces */
    GG_IMPLEMENTS(GG_InputStream);
    GG_IMPLEMENTS(GG_OutputStream);

    /* members */
    unsigned int      reference_count;
    GG_DynamicBuffer* buffer;
    GG_Position       read_offset;
    GG_Position       write_offset;
};

/*--------------------------------------------------------------------*/
GG_Result
GG_MemoryStream_Create(size_t size, GG_MemoryStream** stream)
{
    return GG_MemoryStream_CreateFromBuffer(NULL, size, stream);
}

/*--------------------------------------------------------------------*/
GG_MemoryStream*
GG_MemoryStream_Retain(GG_MemoryStream* self)
{
    ++self->reference_count;
    return self;
}

/*--------------------------------------------------------------------*/
void
GG_MemoryStream_Release(GG_MemoryStream* self)
{
    if (--self->reference_count == 0) {
        GG_DynamicBuffer_Release(self->buffer);
        GG_FreeMemory(self);
    }
}

/*--------------------------------------------------------------------*/
GG_Result
GG_MemoryStream_GetBuffer(GG_MemoryStream*   self,
                          GG_DynamicBuffer** buffer)
{
    *buffer = GG_DynamicBuffer_Retain(self->buffer);
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_MemoryStream_GetInputStream(GG_MemoryStream* self,
                               GG_InputStream** stream)
{
    ++self->reference_count;
    *stream = GG_CAST(self, GG_InputStream);
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_MemoryStream_GetOutputStream(GG_MemoryStream*  self,
                                GG_OutputStream** stream)
{
    ++self->reference_count;
    *stream = GG_CAST(self, GG_OutputStream);
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
static GG_InputStream*
GG_MemoryStream_InputRetain(GG_InputStream* _self)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_InputStream);
    return GG_CAST(GG_MemoryStream_Retain(self), GG_InputStream);
}

/*--------------------------------------------------------------------*/
static void
GG_MemoryStream_InputRelease(GG_InputStream* _self)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_InputStream);
    GG_MemoryStream_Release(self);
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_Read(GG_InputStream* _self,
                     void*            buffer,
                     size_t           bytes_to_read,
                     size_t*          bytes_read)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_InputStream);
    size_t           available;

    /* check for shortcut */
    if (bytes_to_read == 0) {
        if (bytes_read) *bytes_read = 0;
        return GG_SUCCESS;
    }

    /* clip to what's available */
    available = GG_DynamicBuffer_GetDataSize(self->buffer);
    if (self->read_offset+bytes_to_read > available) {
        bytes_to_read = (size_t)(available-self->read_offset);
    }

    /* copy the data */
    if (bytes_to_read) {
        memcpy(buffer, GG_DynamicBuffer_GetData(self->buffer)+self->read_offset, bytes_to_read);
        self->read_offset += bytes_to_read;
    }
    if (bytes_read) *bytes_read = bytes_to_read;

    return bytes_to_read?GG_SUCCESS:GG_ERROR_EOS;
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_InputSeek(GG_InputStream* _self, GG_Position offset)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_InputStream);
    if (offset > GG_DynamicBuffer_GetDataSize(self->buffer)) {
        return GG_ERROR_INVALID_PARAMETERS;
    } else {
        self->read_offset = offset;
        return GG_SUCCESS;
    }
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_InputTell(GG_InputStream* _self, GG_Position* where)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_InputStream);
    if (where) *where = self->read_offset;
    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_MemoryStream_GetSize
+---------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_GetSize(GG_InputStream* _self, GG_Position* size)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_InputStream);
    if (size) *size = GG_DynamicBuffer_GetDataSize(self->buffer);
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_GetAvailable(GG_InputStream* _self,
                              GG_Position*   available)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_InputStream);
    *available = GG_DynamicBuffer_GetDataSize(self->buffer)-self->read_offset;
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
static GG_OutputStream*
GG_MemoryStream_OutputRetain(GG_OutputStream* _self)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_OutputStream);
    return GG_CAST(GG_MemoryStream_Retain(self), GG_OutputStream);
}

/*--------------------------------------------------------------------*/
static void
GG_MemoryStream_OutputRelease(GG_OutputStream* _self)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_OutputStream);
    GG_MemoryStream_Release(self);
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_Write(GG_OutputStream* _self,
                      const void*      data,
                      size_t           bytes_to_write,
                      size_t*          bytes_written)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_OutputStream);

    GG_Result result = GG_DynamicBuffer_Reserve(self->buffer, (size_t)(self->write_offset+bytes_to_write));
    if (GG_FAILED(result)) return result;

    memcpy(GG_DynamicBuffer_UseData(self->buffer)+self->write_offset, data, bytes_to_write);
    self->write_offset += bytes_to_write;
    if (self->write_offset > GG_DynamicBuffer_GetDataSize(self->buffer)) {
        GG_DynamicBuffer_SetDataSize(self->buffer, (size_t)self->write_offset);
    }
    if (bytes_written) *bytes_written = bytes_to_write;

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_OutputSeek(GG_OutputStream* _self, GG_Position offset)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_OutputStream);
    if (offset <= GG_DynamicBuffer_GetDataSize(self->buffer)) {
        self->write_offset = offset;
        return GG_SUCCESS;
    } else {
        return GG_ERROR_INVALID_PARAMETERS;
    }
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_OutputTell(GG_OutputStream* _self, GG_Position* where)
{
    GG_MemoryStream* self = GG_SELF(GG_MemoryStream, GG_OutputStream);
    if (where) *where = self->write_offset;
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
static GG_Result
GG_MemoryStream_Flush(GG_OutputStream* self)
{
    GG_COMPILER_UNUSED(self);
    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_MemoryStream, GG_InputStream) {
    GG_MemoryStream_InputRetain,
    GG_MemoryStream_InputRelease,
    GG_MemoryStream_Read,
    GG_MemoryStream_InputSeek,
    GG_MemoryStream_InputTell,
    GG_MemoryStream_GetSize,
    GG_MemoryStream_GetAvailable
};

/*--------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_MemoryStream, GG_OutputStream) {
    GG_MemoryStream_OutputRetain,
    GG_MemoryStream_OutputRelease,
    GG_MemoryStream_Write,
    GG_MemoryStream_OutputSeek,
    GG_MemoryStream_OutputTell,
    GG_MemoryStream_Flush
};

/*--------------------------------------------------------------------*/
GG_Result
GG_MemoryStream_CreateFromBuffer(void*             buffer,
                                 size_t            size,
                                 GG_MemoryStream** stream)
{
    GG_Result result;

    /* allocate the object */
    *stream = GG_AllocateZeroMemory(sizeof(GG_MemoryStream));
    if (*stream == NULL) return GG_ERROR_OUT_OF_MEMORY;

    /* construct the object */
    if (buffer == NULL) {
        result = GG_DynamicBuffer_Create(size, &(*stream)->buffer);
    } else {
        result = GG_DynamicBuffer_Create(0, &(*stream)->buffer);
    }
    if (GG_FAILED(result)) {
        GG_FreeMemory((void*)(*stream));
        return result;
    }

    /* set the buffer if needed */
    if (buffer != NULL) {
        result = GG_DynamicBuffer_SetBuffer((*stream)->buffer,
                                          buffer,
                                          size);
        if (GG_SUCCEEDED(result)) {
            result = GG_DynamicBuffer_SetDataSize((*stream)->buffer, size);
        }
        if (GG_FAILED(result)) {
            GG_DynamicBuffer_Release((*stream)->buffer);
            GG_FreeMemory((void*)(*stream));
            return result;
        }
    }

    (*stream)->reference_count = 1;
    (*stream)->read_offset     = 0;
    (*stream)->write_offset    = 0;

    /* setup the interfaces */
    GG_SET_INTERFACE(*stream, GG_MemoryStream, GG_InputStream);
    GG_SET_INTERFACE(*stream, GG_MemoryStream, GG_OutputStream);

    return GG_SUCCESS;
}

