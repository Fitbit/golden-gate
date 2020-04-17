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
 * @date 2018-01-09
 *
 * @details
 * Performance-measuring data sink
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_utils.h"
#include "xp/sockets/gg_sockets.h"
#include "gg_print_data_sink.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.utils.print-data-sink")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_PrintDataSink {
    GG_IMPLEMENTS(GG_DataSink);

    uint32_t options;
    size_t   max_payload_print;
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_PRINT_DATA_SINK_PAYLOAD_CHUNK_SIZE 16

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_PrintDataSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_PrintDataSink* self = GG_SELF(GG_PrintDataSink, GG_DataSink);

    // get the packet payload and size
    const uint8_t* packet      = GG_Buffer_GetData(data);
    size_t         packet_size = GG_Buffer_GetDataSize(data);
    GG_LOG_FINEST("got packet, size=%u", (int)packet_size);

    // print the size
    char message[64];
    snprintf(message, sizeof(message), "Packet: %u bytes ", (int)packet_size);
    GG_System_ConsoleOutput(message);

    // print the metadata
    if ((self->options & GG_PRINT_DATA_SINK_OPTION_PRINT_METADATA) &&
        metadata &&
        (metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS ||
         metadata->type == GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS)) {
        const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)metadata;
        snprintf(message,
                 sizeof(message),
                 "[%d.%d.%d.%d:%d] ",
                 (int)socket_metadata->socket_address.address.ipv4[0],
                 (int)socket_metadata->socket_address.address.ipv4[1],
                 (int)socket_metadata->socket_address.address.ipv4[2],
                 (int)socket_metadata->socket_address.address.ipv4[3],
                 (int)socket_metadata->socket_address.port);
        GG_System_ConsoleOutput(message);
    }

    // print the payload
    if (self->max_payload_print) {
        size_t bytes_to_print = GG_MIN(packet_size, self->max_payload_print);
        for (size_t i = 0; i < bytes_to_print; i += GG_PRINT_DATA_SINK_PAYLOAD_CHUNK_SIZE) {
            size_t chunk = GG_PRINT_DATA_SINK_PAYLOAD_CHUNK_SIZE;
            if (i + chunk > bytes_to_print) {
                chunk = bytes_to_print - i;
            }
            GG_BytesToHex(&packet[i], chunk, message, true);
            message[2 * chunk] = '\0';
            GG_System_ConsoleOutput(message);
        }

        if (packet_size > self->max_payload_print) {
            GG_System_ConsoleOutput("...");
        }
    }

    // newline
    GG_System_ConsoleOutput("\r\n");

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_PrintDataSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_PrintDataSink, GG_DataSink) {
    .PutData     = GG_PrintDataSink_PutData,
    .SetListener = GG_PrintDataSink_SetListener
};
//----------------------------------------------------------------------
GG_Result
GG_PrintDataSink_Create(uint32_t           options,
                        size_t             max_payload_print,
                        GG_PrintDataSink** sink)
{
    GG_ASSERT(sink);

    // allocate a new object
    *sink = (GG_PrintDataSink*)GG_AllocateZeroMemory(sizeof(GG_PrintDataSink));
    if (*sink == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object fields
    (*sink)->options           = options;
    (*sink)->max_payload_print = max_payload_print;

    // initialize the object interfaces
    GG_SET_INTERFACE(*sink, GG_PrintDataSink, GG_DataSink);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_PrintDataSink_Destroy(GG_PrintDataSink* self)
{
    if (self == NULL) return;

    // free the object memory
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_PrintDataSink_AsDataSink(GG_PrintDataSink* self)
{
    return GG_CAST(self, GG_DataSink);
}
