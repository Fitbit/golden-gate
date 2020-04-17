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
 * @date 2017-12-08
 *
 * @details
 * Memory sink
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_io.h"
#include "gg_memory_data_sink.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_MemoryDataSink {
    GG_IMPLEMENTS(GG_DataSink);

    GG_DynamicBuffer* buffer;
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_MemoryDataSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_MemoryDataSink* self = GG_SELF(GG_MemoryDataSink, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    return GG_DynamicBuffer_AppendData(self->buffer, GG_Buffer_GetData(data), GG_Buffer_GetDataSize(data));
}

//----------------------------------------------------------------------
static GG_Result
GG_MemoryDataSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_MemoryDataSink, GG_DataSink) {
    .PutData     = GG_MemoryDataSink_PutData,
    .SetListener = GG_MemoryDataSink_SetListener
};

//----------------------------------------------------------------------
GG_Result GG_MemoryDataSink_Create(GG_MemoryDataSink** sink)
{
    // allocate a new object
    *sink = (GG_MemoryDataSink*)GG_AllocateZeroMemory(sizeof(GG_MemoryDataSink));
    if (*sink == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // allocate a buffer
    GG_Result result = GG_DynamicBuffer_Create(0, &(*sink)->buffer);
    if (GG_FAILED(result)) {
        GG_FreeMemory(*sink);
        return result;
    }

    // initialize the object interfaces
    GG_SET_INTERFACE(*sink, GG_MemoryDataSink, GG_DataSink);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_MemoryDataSink_Reset(GG_MemoryDataSink* self)
{
    GG_DynamicBuffer_SetDataSize(self->buffer, 0);
    GG_DynamicBuffer_SetBufferSize(self->buffer, 0);
}

//----------------------------------------------------------------------
void
GG_MemoryDataSink_Destroy(GG_MemoryDataSink* self)
{
    if (self == NULL) return;

    // release the data
    GG_DynamicBuffer_Release(self->buffer);

    // free the object memory
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_MemoryDataSink_AsDataSink(GG_MemoryDataSink* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
GG_Buffer*
GG_MemoryDataSink_GetBuffer(GG_MemoryDataSink* self)
{
    return GG_DynamicBuffer_AsBuffer(self->buffer);
}
