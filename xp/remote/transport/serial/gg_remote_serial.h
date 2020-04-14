/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author
 *
 * @date 2017-11-07
 *
 * @details
 *
 * Remote Serial
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_buffer.h"
#include "xp/remote/gg_remote.h"
#include "gg_remote_serial_io.h"
#include "gg_remote_parser.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

typedef struct {
    GG_IMPLEMENTS(GG_RemoteTransport);
    GG_SerialIO* serial_link;
} SerialTransport;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

GG_Result SerialTransport_ReadFrame(GG_RemoteTransport* self, GG_Buffer** buffer);
GG_Result SerialTransport_WriteFrame(GG_RemoteTransport* self, GG_Buffer* buffer);
GG_Result SerialTransport_SendAck(GG_SerialIO* self, uint8_t* seq);

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

GG_Result GG_SerialTransport_CreateFrame(GG_RemoteTransport* self, GG_Buffer* payload_buff, 
                                      GG_Buffer** frame_buff, uint8_t* crc, uint8_t* seq);
GG_Result GG_SerialTransport_EncodeDecodePayload(GG_Buffer* src, GG_Buffer** dst, bool encode);
size_t GG_SerialTransport_GetFrameSize(GG_SerialIO* self);
GG_Result GG_SerialTransport_GetFramePayload(GG_SerialIO* self, GG_Buffer** data);
const uint8_t* GG_SerialTransport_GetFramePtr(GG_SerialIO* self);
size_t GG_SerialTransport_GetFramePayloadSize(GG_SerialIO* self);
void GG_SerialTransport_ClearParserState(GG_SerialIO* self);
GG_Result GG_SerialTransport_GetFrameSeq(GG_SerialIO* self, uint8_t* seq);

void SerialTransport_Init(SerialTransport* self, GG_SerialIO* serial_link);
