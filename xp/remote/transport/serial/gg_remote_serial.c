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
 * Implements serial transport interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include <string.h>

#include "gg_remote_serial.h"
#include "gg_remote_serial_io.h"
#include "xp/common/gg_crc32.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_buffer.h"
#include "xp/remote/gg_remote.h"
#include "xp/common/gg_utils.h"

#include "fb_smo.h"
#include "fb_smo_serialization.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define RETRY_MAX 5
#define DEFAULT_FRAME_SEQ_NUM_ARRAY {'0', '0', '0', '0', '0', '0', '0', '0'}

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/


/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
// convert 8-byte ascii chars into uint32_t value
static GG_Result
GG_SerialTransport_atoi32(const uint8_t* data_ptr, uint32_t* res)
{
    uint32_t result = 0;
    uint8_t byte = 0;
    if (data_ptr) {
        for (int i = 0; i < 8; i++) {
            byte = data_ptr[i];
            if (byte >= '0' && byte <= '9') byte = byte - '0';
            else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
            else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;
            else return GG_FAILURE;
            result = (result << 4) | (byte & 0xF);
        }
    }
    *res = result;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// convert uint32_t value into 8-byte ascii chars (as hex)
static GG_Result
GG_SerialTransport_itoa_hex(uint32_t source, uint8_t * result_buf)
{
    // Copy CRC, Frame counter
    if (!result_buf) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    static char lookup[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'A', 'B', 'C', 'D', 'E', 'F'};
    int j = 0;
    for (int i = 3; i >= 0; i--) {
        result_buf[j++] = lookup[((source >> 8*i) & 0xF0) >> 4];
        result_buf[j++] = lookup[((source >> 8*i) & 0x0F)];
    }
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_SerialTransport_FrameValidation(SerialTransport* self)
{
    GG_Result result;

    size_t frame_len = GG_SerialTransport_GetFrameSize(self->serial_link);
    const uint8_t* frame_ptr = GG_SerialTransport_GetFramePtr(self->serial_link);
    if (frame_len < GG_MIN_FRAME_LEN || !frame_ptr) {
        return GG_FAILURE;
    }

    uint32_t stored_crc = 0, cal_crc = 0;
    result = GG_SerialTransport_atoi32(frame_ptr + frame_len - GG_CRC_FRAMED_LEN - GG_SEQ_FRAMED_LEN - GG_END_BYTE_LEN,
                                       &stored_crc);
    if (result != GG_SUCCESS) {
        return result;
    }
    
    // calculate crc32 within payload
    cal_crc = GG_Crc32(0, (const void *)&frame_ptr[1], frame_len - GG_MIN_FRAME_LEN);

    return (cal_crc == stored_crc) ? GG_SUCCESS : GG_FAILURE;
}

//----------------------------------------------------------------------
GG_Result
SerialTransport_SendAck(GG_SerialIO* _self, uint8_t* seq)
{
    uint8_t ack_frame[GG_ACK_FRAME_LEN];
    ack_frame[0] = GG_ACK_FRAME_START_BYTE;
    memcpy(&ack_frame[1], seq, GG_SEQ_FRAMED_LEN);

    GG_DynamicBuffer *ack_frame_buff = NULL;
    GG_DynamicBuffer_Create(GG_ACK_FRAME_LEN, &ack_frame_buff);
    GG_DynamicBuffer_SetData(ack_frame_buff, &ack_frame[0], GG_ACK_FRAME_LEN);
    GG_SerialIO_Write(_self, GG_DynamicBuffer_AsBuffer(ack_frame_buff));
    
    GG_DynamicBuffer_Release(ack_frame_buff);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result 
GG_SerialTransport_EncodeDecodePayload(GG_Buffer* src, GG_Buffer** dst, bool encode) {
    GG_Result result;
    size_t dst_size = 0;
    GG_DynamicBuffer* buff = NULL;
    // Get output size first
    if (encode) {
        result = GG_Base64_Encode(GG_Buffer_GetData(src),
                                GG_Buffer_GetDataSize(src),
                                NULL,
                                &dst_size,
                                false);
    } else {
        result = GG_Base64_Decode((const char*)GG_Buffer_GetData(src),
                                GG_Buffer_GetDataSize(src),
                                NULL,
                                &dst_size,
                                false);
    }
    
    if (result != GG_ERROR_NOT_ENOUGH_SPACE) {
        return result;
    }
    // Allocate output buffer
    result = GG_DynamicBuffer_Create(dst_size, &buff);
    GG_DynamicBuffer_SetDataSize(buff, dst_size);

    if (result != GG_SUCCESS) {
        return result;
    }
    // Encode or decode
    if (encode) {
        result = GG_Base64_Encode(GG_Buffer_GetData(src),
                                GG_Buffer_GetDataSize(src),
                                (char*)GG_DynamicBuffer_UseData(buff),
                                &dst_size,
                                false);
    } else {
        result = GG_Base64_Decode((const char*)GG_Buffer_GetData(src),
                                GG_Buffer_GetDataSize(src),
                                GG_DynamicBuffer_UseData(buff),
                                &dst_size,
                                false);
    }

    if (result != GG_SUCCESS) {
        return result;
    }
    // point dst to our output buffer
    *dst = GG_DynamicBuffer_AsBuffer(buff);

    return result;
}

//----------------------------------------------------------------------
GG_Result
GG_SerialTransport_CreateFrame(GG_RemoteTransport* _self, GG_Buffer* payload_buff, GG_Buffer** frame_buff, 
                            uint8_t* crc, uint8_t* seq)
{
    GG_COMPILER_UNUSED(_self);

    size_t frame_len = GG_MIN_FRAME_LEN + GG_Buffer_GetDataSize(payload_buff);

    GG_DynamicBuffer* frame = NULL;
    GG_DynamicBuffer_Create(frame_len, &frame);
    GG_DynamicBuffer_SetDataSize(frame, frame_len);
    uint8_t* frame_ptr = GG_DynamicBuffer_UseData(frame);

    *frame_ptr++ = GG_FRAME_START_BYTE;
    memcpy(frame_ptr, GG_Buffer_GetData(payload_buff), GG_Buffer_GetDataSize(payload_buff));
    frame_ptr += GG_Buffer_GetDataSize(payload_buff);
    *frame_ptr++ = GG_FRAME_PAYLOAD_END_BYTE;
    memcpy(frame_ptr, crc, GG_CRC_FRAMED_LEN);
    frame_ptr += GG_CRC_FRAMED_LEN;
    memcpy(frame_ptr, seq, GG_SEQ_FRAMED_LEN);
    frame_ptr += GG_SEQ_FRAMED_LEN;
    *frame_ptr = GG_FRAME_END_BYTE;

    *frame_buff = GG_DynamicBuffer_AsBuffer(frame);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
SerialTransport_ReadFrame(GG_RemoteTransport* _self, GG_Buffer** buffer)
{
    SerialTransport* self = GG_SELF(SerialTransport, GG_RemoteTransport);
    GG_Buffer* payload_buff = NULL;

    GG_Result result;

    // Frame read and valiation
    for (; ;) {
        result = GG_SerialIO_ReadFrame(self->serial_link, &payload_buff);
        if (result == GG_ERROR_REMOTE_EXIT) {
            return result;
        }

        if ((GG_SUCCESS == result) && 
            (GG_SUCCESS == GG_SerialTransport_FrameValidation(self))) {
            // Send Ack
            uint8_t frame_seq_number[GG_SEQ_FRAMED_LEN];
            GG_SerialTransport_GetFrameSeq(self->serial_link, &frame_seq_number[0]);
            SerialTransport_SendAck(self->serial_link, &frame_seq_number[0]);
        
            // Get Received frame, details
            size_t payload_len = GG_Buffer_GetDataSize(payload_buff);
            uint8_t payload_buffer[GG_MAX_FRAME_LEN];
            memcpy(&payload_buffer[0], GG_Buffer_GetData(payload_buff), payload_len);
        
            // Base64 Decoding
            GG_Buffer* cbor_buff = NULL;
            result = GG_SerialTransport_EncodeDecodePayload(payload_buff, &cbor_buff, false);

            if (result != GG_SUCCESS) {
                // Invalid payload; Clear state and continue
                GG_SerialTransport_ClearParserState(self->serial_link);
                continue;
            }

            *buffer  = cbor_buff;

            GG_SerialTransport_ClearParserState(self->serial_link);
            break;
        } else {
            GG_SerialTransport_ClearParserState(self->serial_link);
        }
    }
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
SerialTransport_WriteFrame(GG_RemoteTransport* _self, GG_Buffer* buffer)
{
    SerialTransport* self = GG_SELF(SerialTransport, GG_RemoteTransport);
    GG_Result result;
    GG_Buffer* encoded_payload = NULL;
    GG_Buffer* frame = NULL;

    result = GG_SerialTransport_EncodeDecodePayload(buffer, &encoded_payload, true);
    if (result != GG_SUCCESS) {
        return result;
    }

    uint32_t cal_crc = GG_Crc32(0, GG_Buffer_GetData(encoded_payload), GG_Buffer_GetDataSize(encoded_payload));
    uint8_t crc[GG_CRC_FRAMED_LEN] = {0};    
    GG_SerialTransport_itoa_hex(cal_crc, &crc[0]);
    uint8_t canned_frame_counter[GG_SEQ_FRAMED_LEN] = DEFAULT_FRAME_SEQ_NUM_ARRAY;

    GG_SerialTransport_CreateFrame(_self, encoded_payload, &frame, &crc[0], &canned_frame_counter[0]);

    for (uint8_t retry = 0; retry < RETRY_MAX; retry++) {
        GG_SerialIO_Write(self->serial_link, frame);

        if (GG_SUCCESS == GG_SerialIO_ReadAck(self->serial_link)) {
            break;
        }
    }

    GG_Buffer_Release(encoded_payload);
    GG_Buffer_Release(frame);

    GG_SerialTransport_ClearParserState(self->serial_link);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
//  Other functions
//----------------------------------------------------------------------
size_t
GG_SerialTransport_GetFrameSize(GG_SerialIO* _self) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    size_t frame_len = 0;
    GG_SerialRemoteParser_GetFrameLen(self->parser, &frame_len);

    return frame_len;
}

//----------------------------------------------------------------------
GG_Result
GG_SerialTransport_GetFramePayload(GG_SerialIO* _self, GG_Buffer** data) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    return GG_SerialRemoteParser_GetFramePayload(self->parser, data);
}

//----------------------------------------------------------------------
const uint8_t*
GG_SerialTransport_GetFramePtr(GG_SerialIO* _self) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    return GG_SerialRemoteParser_GetFramePtr(self->parser);
}

//----------------------------------------------------------------------
size_t 
GG_SerialTransport_GetFramePayloadSize(GG_SerialIO* _self) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    return GG_SerialRemoteParser_GetFramePayloadLen(self->parser);
}

//----------------------------------------------------------------------
void
GG_SerialTransport_ClearParserState(GG_SerialIO* _self) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    GG_SerialRemoteParser_Reset(self->parser);
}

//----------------------------------------------------------------------
GG_Result
GG_SerialTransport_GetFrameSeq(GG_SerialIO* _self, uint8_t* seq) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    return GG_SerialRemoteParser_GetFrameSeq(self->parser, seq);
}

//----------------------------------------------------------------------

GG_IMPLEMENT_INTERFACE(SerialTransport, GG_RemoteTransport) {
    SerialTransport_WriteFrame,
    SerialTransport_ReadFrame
};

//----------------------------------------------------------------------

void
SerialTransport_Init(SerialTransport* transport, GG_SerialIO* serial_link)
{
     // init all the fields to 0
    memset(transport, 0, sizeof(*transport));
    
    transport->serial_link = serial_link;

    // setup interfaces
    GG_SET_INTERFACE(transport, SerialTransport, GG_RemoteTransport);
}
