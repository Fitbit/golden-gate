/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-08
 *
 * @details
 * Memory data source
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_memory_data_source.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.utils.memory-data-source")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_MemoryDataSource {
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSinkListener);

    GG_DataSink* sink;
    GG_Buffer*   data;
    size_t       data_offset;
    size_t       chunk_size;
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_MemoryDataSource_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_MemoryDataSource* self = GG_SELF(GG_MemoryDataSource, GG_DataSource);

    // de-register as a listener from the current sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    // keep a reference to the new sink
    self->sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(self, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_MemoryDataSource_OnCanPut(GG_DataSinkListener* _self)
{
    GG_MemoryDataSource* self = GG_SELF(GG_MemoryDataSource, GG_DataSinkListener);

    // check that we have a sink
    if (!self->sink) {
        return;
    }

    // send as much as we can until we're blocked
    GG_Result result;
    do {
        // create a buffer for the next chunk
        size_t bytes_left = GG_MemoryDataSource_GetBytesLeft(self);
        size_t chunk = GG_MIN(self->chunk_size, bytes_left);
        if (chunk == 0) {
            // no data left
            return;
        }
        GG_Buffer* buffer = NULL;
        result = GG_SubBuffer_Create(self->data, self->data_offset, chunk, &buffer);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("GG_SubBuffer_Create failed (%d)", result);
            return;
        }

        // try to deliver the buffer
        result = GG_DataSink_PutData(self->sink, buffer, NULL);
        if (GG_SUCCEEDED(result)) {
            self->data_offset += chunk;
        }
        GG_Buffer_Release(buffer);
    } while (GG_SUCCEEDED(result));
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_MemoryDataSource, GG_DataSinkListener) {
    .OnCanPut = GG_MemoryDataSource_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_MemoryDataSource, GG_DataSource) {
    .SetDataSink = GG_MemoryDataSource_SetDataSink
};

//----------------------------------------------------------------------
GG_Result GG_MemoryDataSource_Create(GG_Buffer* data, size_t chunk_size, GG_MemoryDataSource** source)
{
    GG_ASSERT(data);

    // allocate a new object
    *source = (GG_MemoryDataSource*)GG_AllocateZeroMemory(sizeof(GG_MemoryDataSource));
    if (*source == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object fields
    (*source)->chunk_size = chunk_size;
    (*source)->data       = GG_Buffer_Retain(data);

    // initialize the object interfaces
    GG_SET_INTERFACE(*source, GG_MemoryDataSource, GG_DataSource);
    GG_SET_INTERFACE(*source, GG_MemoryDataSource, GG_DataSinkListener);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_MemoryDataSource_Destroy(GG_MemoryDataSource* self)
{
    if (self == NULL) return;

    // de-register as a listener from the sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    // release the data
    GG_Buffer_Release(self->data);

    // free the object memory
    GG_ClearAndFreeObject(self, 2);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_MemoryDataSource_AsDataSource(GG_MemoryDataSource* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_Result
GG_MemoryDataSource_Start(GG_MemoryDataSource* self)
{
    // prime the pump
    GG_MemoryDataSource_OnCanPut(GG_CAST(self, GG_DataSinkListener));

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
size_t
GG_MemoryDataSource_GetBytesLeft(GG_MemoryDataSource* self)
{
    return GG_Buffer_GetDataSize(self->data) - self->data_offset;
}

//----------------------------------------------------------------------
void
GG_MemoryDataSource_Rewind(GG_MemoryDataSource* self)
{
    self->data_offset = 0;
}
