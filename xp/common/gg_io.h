/**
 * @file
 * @brief General purpose I/O interfaces
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

#include "xp/common/gg_types.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_strings.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup InputOutput I/O
//! General Purpose I/O Interfaces, Classes and Functions
//! @{

/*----------------------------------------------------------------------
|   error codes
+---------------------------------------------------------------------*/
#define GG_ERROR_EOS (GG_ERROR_BASE_IO - 0) ///< End Of Stream

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
//!
//! Base class for buffer metadata sub-classes.
//! NOTE: subclasses must be clonable by simple memory copy
//!
typedef struct {
    uint32_t type; ///< Unique type identifier.
    size_t   size; ///< Size of the struct
} GG_BufferMetadata;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_BUFFER_METADATA_TYPE_NONE 0

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/

//!
//! Initialize a GG_BufferMetadata struct given its type ID and C type
//!
//! The type ID passed here is the tail of the C constant name for the
//! type ID that follows the GG_BUFFER_METADATA_TYPE_ prefix. So for
//! example, to initialize a metadata struct that has the C type FooBar
//! and the type ID constant GG_BUFFER_METADATA_TYPE_FOOBAR, you would
//! use GG_BUFFER_METADATA_INITIALIZER(FOOBAR, Foobar)
//!
#define GG_BUFFER_METADATA_INITIALIZER(_type_id, _c_type) \
((GG_BufferMetadata){                                     \
    .type = GG_BUFFER_METADATA_TYPE_##_type_id,           \
    .size = sizeof(_c_type)                               \
})

//----------------------------------------------------------------------
//! Clone a metadata object
//!
//! @param metadata The object on which this method is called.
//! @param clone Address of the variable where the cloned object will be returned.
//!
//! @return GG_SUCCESS if the object could be cloned, or a negative error code.
//----------------------------------------------------------------------
GG_Result GG_BufferMetadata_Clone(const GG_BufferMetadata* metadata, GG_BufferMetadata** clone);

//----------------------------------------------------------------------
//! Interface implemented by objects that need to be called when they can call GG_DataSink_PutData
//!
//! Typically used to be notified after calling GG_DataSink_PutData and getting a result equal
//! to #GG_ERROR_WOULD_BLOCK.
//!
//! @interface GG_DataSinkListener
//! @see GG_DataSink
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_DataSinkListener) {
    /**
     * Notify the object that it can/should call its sink's GG_DataSink_PutData
     * method again.
     *
     * @param self The object on which this method is invoked.
     */
    void (*OnCanPut)(GG_DataSinkListener* self);
};

//! @var GG_DataSinkListener::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_DataSinkListenerInterface
//! Virtual function table for the GG_DataSinkListener interface

//! @relates GG_DataSinkListener
//! @copydoc GG_DataSinkListenerInterface::OnCanPut
void GG_DataSinkListener_OnCanPut(GG_DataSinkListener* self);

//----------------------------------------------------------------------
//! Interface implemented by objects that can receive data.
//!
//! @interface GG_DataSink
//! @see GG_DataSource
//! @see GG_DataSinkListener
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_DataSink) {
    /**
     * Put data to a sink.
     * If the data is accepted, the caller may release its reference to the
     * data passed in, since the sink will retain its own references if needed.
     *
     * @param self The object on which this method is invoked.
     * @param data The data to put.
     * @param metadata Optional metadata (may be NULL).
     * @return #GG_SUCCESS if the data was accepted by the sink, #GG_ERROR_WOULD_BLOCK
     * if the sink would normally accept the data, but cannot do so at this time,
     * in which case the caller should retry at a later time.
     * @see GG_DataSinkListenerInterface::OnCanPut
     */
    GG_Result (*PutData)(GG_DataSink* self, GG_Buffer* data, const GG_BufferMetadata* metadata);

    /**
     * Set a sink's listener.
     * The listener is called when it is appropriate to call GG_DataSink_PutData
     * after a return value equal to #GG_ERROR_WOULD_BLOCK
     *
     * @param self The object on which this method is invoked.
     * @param listener The listener for the sink.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*SetListener)(GG_DataSink* self, GG_DataSinkListener* listener);
};

//! @var GG_DataSink::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_DataSinkInterface
//! Virtual function table for the GG_DataSink interface

//! @relates GG_DataSink
//! @copydoc GG_DataSinkInterface::PutData
GG_Result GG_DataSink_PutData(GG_DataSink* self, GG_Buffer* data, const GG_BufferMetadata* metadata);

//! @relates GG_DataSink
//! @copydoc GG_DataSinkInterface::SetListener
GG_Result GG_DataSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener);

//----------------------------------------------------------------------
//! Interface implemented by objects that need to send data to a sink
//!
//! @interface GG_DataSource
//! @see GG_DataSink
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_DataSource) {
    /**
     * Set the sink to which this source will send data.
     *
     * NOTE: the lifetime of the sink passed to this method must be the same or longer
     * than that of this object. Therefore, if that sink must be destroyed earlier than
     * this object, the caller MUST call this method again with a NULL (or other sink)
     * before destroying it.
     *
     * NOTE: objects that implement this method typically call the sink's GG_DataSink::SetListener
     * method to register themselves as a listener. In that case, the object MUST keep a reference
     * to the sink so it can de-register itself by calling GG_DataSink::SetListener(_, NULL) prior to
     * registering itself to a different sink and/or in its destructor.
     *
     * @param self The object on which this method is invoked.
     * @param sink The sink that will receive data from this object when available.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*SetDataSink)(GG_DataSource* self, GG_DataSink* sink);
};

//! @var GG_DataSource::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_DataSourceInterface
//! Virtual function table for the GG_DataSource interface

//! @relates GG_DataSource
//! @copydoc GG_DataSourceInterface::SetDataSink
GG_Result GG_DataSource_SetDataSink(GG_DataSource* self, GG_DataSink* sink);

//----------------------------------------------------------------------
//! Interface implemented by objects from which data can be read as a stream.
//!
//! @interface GG_InputStream
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_InputStream) {
    /**
     * Retain a reference to this object.
     *
     * @param self The object on which this method is invoked.
     * @return The retained object (which may or may not be the same pointer as the
     * one on which this method is called).
     */
    GG_InputStream* (*Retain)(GG_InputStream* self);

    /**
     * Release a reference to this object.
     *
     * @param self The object on which this method is invoked.
     */
    void (*Release)(GG_InputStream* self);

    /**
     * Read data.
     *
     * @param self The object on which this method is invoked.
     * @param buffer Buffer in which the data should be read.
     * @param bytes_to_read Maximum number of bytes to read.
     * @param bytes_read Pointer to the variable in which the number of bytes read will be returned.
     * @return #GG_SUCCESS if at least one byte was read, #GG_ERROR_EOS if the end of the stream
     *         has been reached, or an error code.
     */
    GG_Result (*Read)(GG_InputStream* self,
                      void*           buffer,
                      size_t          bytes_to_read,
                      size_t*         bytes_read);

    /**
     * Change the current stream position, relative to the start of the stream.
     *
     * @param self The object on which this method is invoked.
     * @param offset Position to which to seek.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*Seek)(GG_InputStream* self, GG_Position offset);

    /**
     * Get the current stream position, relative to the start of the stream.
     *
     * @param self The object on which this method is invoked.
     * @param offset Pointer to a variable in which the position will be returned.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*Tell)(GG_InputStream* self, GG_Position* offset);

    /**
     * Get the size of the stream.
     *
     * @param self The object on which this method is invoked.
     * @param size Pointer to a variable in which the size will be returned.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*GetSize)(GG_InputStream* self, GG_Position* size);

    /**
     * Get the amount of data that can be read from the stream, from the current position.
     *
     * @param self The object on which this method is invoked.
     * @param available Pointer to a variable in which the number of bytes available to read will be returned.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*GetAvailable)(GG_InputStream* self, GG_Position* available);
};

//! @var GG_InputStream::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_InputStreamInterface
//! Virtual function table for the GG_InputStream interface

//! @relates GG_InputStream
//! @copydoc GG_InputStreamInterface::Retain
GG_InputStream* GG_InputStream_Retain(GG_InputStream* self);

//! @relates GG_InputStream
//! @copydoc GG_InputStreamInterface::Release
void GG_InputStream_Release(GG_InputStream* self);

//! @relates GG_InputStream
//! @copydoc GG_InputStreamInterface::Read
GG_Result GG_InputStream_Read(GG_InputStream* self,
                              void*           buffer,
                              size_t          bytes_to_read,
                              size_t*         bytes_read);

//! @relates GG_InputStream
//! @copydoc GG_InputStreamInterface::Seek
GG_Result GG_InputStream_Seek(GG_InputStream* self, GG_Position offset);

//! @relates GG_InputStream
//! @copydoc GG_InputStreamInterface::Tell
GG_Result GG_InputStream_Tell(GG_InputStream* self, GG_Position* offset);

//! @relates GG_InputStream
//! @copydoc GG_InputStreamInterface::GetSize
GG_Result GG_InputStream_GetSize(GG_InputStream* self, GG_Position* size);

//! @relates GG_InputStream
//! @copydoc GG_InputStreamInterface::GetAvailable
GG_Result GG_InputStream_GetAvailable(GG_InputStream* self, GG_Position* available);

/**
 * Read from a stream one line at a time, into a buffer.
 *
 * @param self The object on which this method is invoked.
 * @param line Buffer in which the line will be read.
 * @param line_size Size of the buffer.
 * @param chars_read Pointer to a variable in which the number of line characters have been read.
 * @return #GG_SUCCESS if a line was read, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_ReadLine(GG_InputStream* self,
                                  char*           line,
                                  size_t          line_size,
                                  size_t*         chars_read);

/**
 * Read from a stream one line at a time, into a string object.
 *
 * @param self The object on which this method is invoked.
 * @param string GG_String object that will receive the line.
 * @param max_length Maximum line size to accept.
 * @return #GG_SUCCESS if a line was read, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_ReadLineString(GG_InputStream* self,
                                        GG_String*      string,
                                        size_t          max_length);

/**
 * Read a 64-bit integer, in Big Endian byte order.
 *
 * @param self The object on which this method is invoked.
 * @param value Pointer to the variable in which the integer will be stored.
 * @return #GG_SUCCESS if a value was read, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_ReadUI64(GG_InputStream* self, uint64_t* value);

/**
 * Read a 32-bit integer, in Big Endian byte order.
 *
 * @param self The object on which this method is invoked.
 * @param value Pointer to the variable in which the integer will be stored.
 * @return #GG_SUCCESS if a value was read, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_ReadUI32(GG_InputStream* self, uint32_t* value);

/**
 * Read a 16-bit integer, in Big Endian byte order.
 *
 * @param self The object on which this method is invoked.
 * @param value Pointer to the variable in which the integer will be stored.
 * @return #GG_SUCCESS if a value was read, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_ReadUI16(GG_InputStream* self, uint16_t* value);

/**
 * Read a fixed amount of data.
 *
 * @param self The object on which this method is invoked.
 * @param buffer Buffer in which the data will be read.
 * @param bytes_to_read Number of bytes to read.
 * @return #GG_SUCCESS if exactly bytes_to_read bytes were read, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_ReadFully(GG_InputStream* self,
                                   void*           buffer,
                                   size_t          bytes_to_read);

/**
 * Skip some data from the current position.
 *
 * @param self The object on which this method is invoked.
 * @param count Number of bytes to skip.
 * @return #GG_SUCCESS if the call succeeded, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_Skip(GG_InputStream* self, size_t count);

/**
 * Read data into a GG_DynamicBuffer buffer.
 *
 * @param self The object on which this method is invoked.
 * @param max_read Maximum number of bytes to read, or 0 to read until the end of the stream.
 * @param buffer Pointer to a variable in which a GG_DynamicBuffer instance will be returned.
 * @return #GG_SUCCESS if the call succeeded, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_InputStream_Load(GG_InputStream*    self,
                              size_t             max_read, /* = 0 if no limit */
                              GG_DynamicBuffer** buffer);

//----------------------------------------------------------------------
//! Interface implemented by objects to which data can be written as a stream.
//!
//! @interface GG_OutputStream
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_OutputStream) {
    /**
     * Retain a reference to this object.
     *
     * @param self The object on which this method is invoked.
     * @return The retained object (which may or may not be the same pointer as the
     * one on which this method is called).
     */
    GG_OutputStream* (*Retain)(GG_OutputStream* self);

    /**
     * Release a reference to this object.
     *
     * @param self The object on which this method is invoked.
     */
    void (*Release)(GG_OutputStream* self);

    /**
     * Write data.
     *
     * @param self The object on which this method is invoked.
     * @param buffer Data to write.
     * @param bytes_to_write Maximum number of bytes to write.
     * @param bytes_written Pointer to a variable in which the number of bytes written is returned.
     * @return #GG_SUCCESS if at least one byte could be written, or an error code.
     */
    GG_Result (*Write)(GG_OutputStream* self,
                       const void*      buffer,
                       size_t           bytes_to_write,
                       size_t*          bytes_written);

    /**
     * Change the current stream position, relative to the start of the stream.
     *
     * @param self The object on which this method is invoked.
     * @param offset Position to which to seek.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*Seek)(GG_OutputStream* self, GG_Position offset);

    /**
     * Get the current stream position, relative to the start of the stream.
     *
     * @param self The object on which this method is invoked.
     * @param offset Pointer to a variable in which the position will be returned.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*Tell)(GG_OutputStream* self, GG_Position* offset);

    /**
     * Flush any pending/cached data.
     *
     * @param self The object on which this method is invoked.
     * @return #GG_SUCCESS if the operation succeeded, or an error code.
     */
    GG_Result (*Flush)(GG_OutputStream* self);
};

//! @var GG_OutputStream::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_OutputStreamInterface
//! Virtual function table for the GG_OutputStream interface

//! @relates GG_OutputStream
//! @copydoc GG_OutputStreamInterface::Retain
GG_OutputStream* GG_OutputStream_Retain(GG_OutputStream* self);

//! @relates GG_OutputStream
//! @copydoc GG_OutputStreamInterface::Retain
void GG_OutputStream_Release(GG_OutputStream* self);

//! @relates GG_OutputStream
//! @copydoc GG_OutputStreamInterface::Write
GG_Result GG_OutputStream_Write(GG_OutputStream* self,
                                const void*      buffer,
                                size_t           bytes_to_write,
                                size_t*          bytes_written);

//! @relates GG_OutputStream
//! @copydoc GG_OutputStreamInterface::Seek
GG_Result GG_OutputStream_Seek(GG_OutputStream* self, GG_Position offset);

//! @relates GG_OutputStream
//! @copydoc GG_OutputStreamInterface::Tell
GG_Result GG_OutputStream_Tell(GG_OutputStream* self, GG_Position* offset);

//! @relates GG_OutputStream
//! @copydoc GG_OutputStreamInterface::Flush
GG_Result GG_OutputStream_Flush(GG_OutputStream* self);

/**
 * Write a fixed amount of data.
 *
 * @param self The object on which this method is invoked.
 * @param buffer Data to write.
 * @param bytes_to_write Number of bytes to write.
 * @return #GG_SUCCESS if exactly bytes_to_write bytes were written, or an error code.
 *
 * @relates GG_OutputStream
 */
GG_Result GG_OutputStream_WriteFully(GG_OutputStream* self,
                                     const void*      buffer,
                                     size_t           bytes_to_write);

/**
 * Write a NULL-terminated string (the NULL termination is not written).
 *
 * @param self The object on which this method is invoked.
 * @param string The string to write.
 * @return #GG_SUCCESS if the entire string was written, or an error code.
 *
 * @relates GG_OutputStream
 */
GG_Result GG_OutputStream_WriteString(GG_OutputStream* self, const char* string);

/**
 * Write a NULL-terminated string followed by `\r\n` (the NULL termination is not written).
 *
 * @param self The object on which this method is invoked.
 * @param line The line to write.
 * @return #GG_SUCCESS if the entire line was written, or an error code.
 *
 * @relates GG_OutputStream
 */
GG_Result GG_OutputStream_WriteLine(GG_OutputStream* self, const char* line);

//---------------------------------------------------------------------
//! @class GG_MemoryStream
//! @implements GG_InputStream
//! @implements GG_OutputStream
//
//! Class that implements the GG_InputStream and GG_OutputStream interfaces,
//! to offer read/write operations on a memory buffer.
//---------------------------------------------------------------------
typedef struct GG_MemoryStream GG_MemoryStream;

/**
 * Create a new instance of GG_MemoryStream.
 * The object starts with a data size of 0.
 * Writing to the GG_OutputStream interface will attempt to grow the data buffer
 * held by the object.
 *
 * @param size The initial size of the memory buffer managed by the object.
 * @param stream Pointer to a variable where the object will be returned.
 * @return #GG_SUCCESS if the object could be created, or an error code.
 *
 * @relates GG_OutputStream
 */
GG_Result GG_MemoryStream_Create(size_t size, GG_MemoryStream** stream);

/**
 * Create a new instance of GG_MemoryStream.
 * The object starts with the supplied data, provided as an external buffer, which
 * must remain allocated during the lifetime of this object.
 * Writing to the GG_OutputStream interface will not attempt to grow the data buffer
 * held by the object.
 *
 * @param buffer The buffer to which this object provides a view.
 * @param size The size of buffer.
 * @param stream Pointer to a variable where the object will be returned.
 * @return #GG_SUCCESS if the object could be created, or an error code.
 *
 * @relates GG_OutputStream
 */
GG_Result GG_MemoryStream_CreateFromBuffer(void*             buffer,
                                           size_t            size,
                                           GG_MemoryStream** stream);

/**
 * @relates GG_MemoryStream
 * @see GG_InputStreamInterface::Retain
 * @see GG_OutputStreamInterface::Retain
 */
GG_MemoryStream* GG_MemoryStream_Retain(GG_MemoryStream* self);

/**
 * @relates GG_MemoryStream
 * @see GG_InputStreamInterface::Release
 * @see GG_OutputStreamInterface::Release
 */
void GG_MemoryStream_Release(GG_MemoryStream* self);

/**
 * Obtain the internal GG_DynamicBuffer object that represents the data managed by this object.
 *
 * @param self The object on which this method is invoked.
 * @param buffer Pointer to a variable where the internal buffer object will be returned.
 * @return #GG_SUCCESS if the call succeeded, or an error code.
 *
 * @relates GG_OutputStream
 */
GG_Result GG_MemoryStream_GetBuffer(GG_MemoryStream* self, GG_DynamicBuffer** buffer);

/**
 * Obtain the GG_InputStream interface for this object.
 * NOTE: the caller must call GG_InputStream_Release on the interface obtained from
 * this call when it is done with it.
 *
 * @param self The object on which this method is invoked.
 * @param stream Pointer to a variable where the GG_InputStream interface will be returned.
 * @return #GG_SUCCESS if the call succeeded, or an error code.
 *
 * @relates GG_InputStream
 */
GG_Result GG_MemoryStream_GetInputStream(GG_MemoryStream* self, GG_InputStream** stream);

/**
 * Obtain the GG_OutputStream interface for this object.
 * NOTE: the caller must call GG_OutputStream_Release on the interface obtained from
 * this call when it is done with it.
 *
 * @param self The object on which this method is invoked.
 * @param stream Pointer to a variable where the GG_OutputStream interface will be returned.
 * @return #GG_SUCCESS if the call succeeded, or an error code.
 *
 * @relates GG_OutputStream
 */
GG_Result GG_MemoryStream_GetOutputStream(GG_MemoryStream* self, GG_OutputStream** stream);

//!@}

#ifdef __cplusplus
}
#endif /* __cplusplus */
