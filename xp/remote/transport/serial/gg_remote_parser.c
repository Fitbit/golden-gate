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
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "xp/remote/gg_remote.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"

#include "xp/remote/transport/serial/gg_remote_parser.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

GG_Result GG_SerialRemoteParser_PutData(GG_SerialRemoteParser* parser, char c)
{
    switch (c) {
        case GG_ACK_FRAME_START_BYTE:
            parser->buffer_idx = 0;
            parser->ack_parsing_started = true;
            parser->ack_parsing_complete = false;
            parser->buffer[parser->buffer_idx++] = c;
            parser->ack_frame_len = 1;
            break;

        case GG_FRAME_START_BYTE:
            parser->buffer_idx = 0;
            parser->frame_parsing_started = true;
            parser->frame_parsing_complete = false;
            parser->buffer[parser->buffer_idx++] = c;
            parser->frame_len = 1;
            break;

        case GG_FRAME_END_BYTE:
            if (parser->frame_parsing_started) {
                parser->buffer[parser->buffer_idx++] = c;
                parser->frame_len += 1;
                parser->frame_parsing_started = false;
                parser->frame_parsing_complete = true;
            } else {
                // Ignoring end byte seen
            }
            break;

        case GG_SHELL_EXIT:
            parser->shell_exit_state = true;
            return GG_ERROR_REMOTE_EXIT;

        default:
            if (parser->ack_parsing_started) {
                parser->buffer[parser->buffer_idx++] = c;
                parser->ack_frame_len += 1;
                if (parser->ack_frame_len >= GG_ACK_FRAME_LEN) {
                    parser->ack_parsing_started = false;
                    parser->ack_parsing_complete = true;
                }
            } else if (parser->frame_parsing_started) {
                parser->buffer[parser->buffer_idx++] = c;
                parser->frame_len += 1;
                if (c == GG_FRAME_PAYLOAD_END_BYTE) {
                    // We parsed from frame start to payload end byte
                    // hence payload_len = current frame_len - 2;
                    parser->payload_len = parser->frame_len - 2;
                }
            } else {
                // Ignore byte seen
            }
            break;
    }
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
bool GG_SerialRemoteParser_IsAckReceived(GG_SerialRemoteParser *parser)
{
    return parser->ack_parsing_complete;
}

//----------------------------------------------------------------------
bool GG_SerialRemoteParser_IsFrameReceived(GG_SerialRemoteParser *parser) {
    return parser->frame_parsing_complete;
}

//----------------------------------------------------------------------
GG_Result GG_SerialRemoteParser_GetFramePayload(GG_SerialRemoteParser *parser,
    GG_Buffer **data)
{
    GG_DynamicBuffer *buf;
    GG_Result ret;

    if (!parser->frame_parsing_complete) {
        return GG_ERROR_INVALID_STATE;
    }

    ret = GG_DynamicBuffer_Create(parser->payload_len, &buf);

    if (ret != GG_SUCCESS) {
        return ret;
    }

    ret = GG_DynamicBuffer_SetData(buf, parser->buffer + 1,
                                        parser->payload_len);

    if (ret != GG_SUCCESS) {
        return ret;
    }

    *data = GG_DynamicBuffer_AsBuffer(buf);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
size_t GG_SerialRemoteParser_GetFramePayloadLen(GG_SerialRemoteParser *parser)
{
    if (!parser->frame_parsing_complete) {
        return 0;
    }

    return parser->payload_len;
}

//----------------------------------------------------------------------
GG_Result GG_SerialRemoteParser_GetFrameSeq(GG_SerialRemoteParser *parser,
    uint8_t *seq)
{
    size_t seq_start;

    if (!parser->frame_parsing_complete) {
        return GG_ERROR_INVALID_STATE;
    }

    seq_start = GG_START_BTYE_LEN + parser->payload_len + GG_PAYLOAD_END_BTYE_LEN + GG_CRC_FRAMED_LEN;
    memcpy(seq, parser->buffer + seq_start, GG_SEQ_FRAMED_LEN);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result GG_SerialRemoteParser_GetFrameLen(GG_SerialRemoteParser *parser, size_t* frame_len)
{
    // GG_SerialRemoteParser* parser = GG_SELF(GG_SerialRemoteParser, GG_RemoteParser);

    if (!parser->frame_parsing_complete) {
        return GG_ERROR_INVALID_STATE;
    }

    *frame_len = parser->frame_len;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
const uint8_t* GG_SerialRemoteParser_GetFramePtr(GG_SerialRemoteParser *parser)
{
    if (!parser->frame_parsing_complete) {
        return NULL;
    }

    return (const uint8_t*)&(parser->buffer[0]);
}

//----------------------------------------------------------------------
GG_Result GG_SerialRemoteParser_Reset(GG_SerialRemoteParser *parser)
{
    // Buffer and index to store the data being parsed
    memset(&(parser->buffer[0]), 0, GG_MAX_FRAME_LEN);
    parser->buffer_idx = 0;
    parser->ack_frame_len = 0;
    parser->ack_parsing_started = false;
    parser->ack_parsing_complete = false;
    parser->frame_len = 0;
    parser->payload_len = 0;
    parser->frame_parsing_started = false;
    parser->frame_parsing_complete = false;
    parser->shell_exit_state = false;

    return GG_SUCCESS;
}


//----------------------------------------------------------------------
void GG_SerialRemoteParser_PrintState(GG_SerialRemoteParser* parser) {
    // Debug printing can go here, but the print/logs are platform speicific
    GG_ASSERT(parser);
}
