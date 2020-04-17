#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stdlib.h>

#include "xp/common/gg_buffer.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_MAX_FRAME_LEN        (512)

#define GG_SEQ_LEN              (4) // 4 Byte Seq number
#define GG_SEQ_FRAMED_LEN       (2 * GG_SEQ_LEN) // len of SEQ as hexa chars
#define GG_CRC_LEN              (4) // CRC 32
#define GG_CRC_FRAMED_LEN       (2 * GG_CRC_LEN) // len of CRC as hexa chars

#define GG_START_BYTE_LEN       (1)
#define GG_PAYLOAD_END_BYTE_LEN (1)
#define GG_END_BYTE_LEN         (1)
#define GG_MIN_FRAME_LEN        (GG_START_BYTE_LEN +\
                                GG_PAYLOAD_END_BYTE_LEN +\
                                GG_CRC_FRAMED_LEN +\
                                GG_SEQ_FRAMED_LEN +\
                                GG_END_BYTE_LEN)
                                // length for an empty frame
                                // Start Byte + Payload End Byte +
                                // CRC + Seq Num + End byte
#define GG_ACK_START_BYTE_LEN   (1)
#define GG_ACK_FRAME_LEN        (GG_ACK_START_BYTE_LEN + GG_SEQ_FRAMED_LEN) // ACK Start byte +
                                                                   // Seq Num

#define GG_FRAME_START_BYTE        '#'
#define GG_FRAME_PAYLOAD_END_BYTE  '$'
#define GG_FRAME_END_BYTE          '~'
#define GG_ACK_FRAME_START_BYTE    '@'
#define GG_SHELL_EXIT              '^'

#define GG_ACK_FRAME_TIMEOUT    (5000)

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/

#define GG_CRC_OFFSET(payload_len) (GG_START_BYTE_LEN + payload_len + GG_PAYLOAD_END_BYTE_LEN)
#define GG_SEQ_OFFSET(payload_len) (CRC_OFFSET(payload_len) + GG_CRC_FRAMED_LEN)
#define GG_FRAME_END_BYTE_OFFSET(payload_len) (SEQ_OFFSET(payload) + GG_SEQ_FRAMED_LEN)

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    // GG_IMPLEMENTS(GG_SerialRemoteParser);

    // Buffer and index to store the data being parsed
    uint8_t buffer[GG_MAX_FRAME_LEN];
    size_t buffer_idx;

    // ACK state
    size_t ack_frame_len;
    bool ack_parsing_started;
    bool ack_parsing_complete;

    // Frame state
    size_t frame_len;
    size_t payload_len;
    bool frame_parsing_started;
    bool frame_parsing_complete;

    // Shell state
    bool shell_exit_state;
} GG_SerialRemoteParser;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
GG_Result GG_SerialRemoteParser_PutData(GG_SerialRemoteParser *self, char c);
bool GG_SerialRemoteParser_IsAckReceived(GG_SerialRemoteParser *self);
bool GG_SerialRemoteParser_IsFrameReceived(GG_SerialRemoteParser *self);
bool GG_SerialRemoteParser_IsShellExitState(GG_SerialRemoteParser *self);
GG_Result GG_SerialRemoteParser_GetFramePayload(GG_SerialRemoteParser *self,
                                          GG_Buffer **data);
size_t GG_SerialRemoteParser_GetFramePayloadLen(GG_SerialRemoteParser *self);
GG_Result GG_SerialRemoteParser_GetFrameSeq(GG_SerialRemoteParser *self, uint8_t *seq);
GG_Result GG_SerialRemoteParser_GetFrameLen(GG_SerialRemoteParser* self, size_t* frame_len);
const uint8_t* GG_SerialRemoteParser_GetFramePtr(GG_SerialRemoteParser *self);
GG_Result GG_SerialRemoteParser_Reset(GG_SerialRemoteParser *self);
void GG_SerialRemoteParser_PrintState(GG_SerialRemoteParser *self);

void GG_SerialRemoteParser_Init(GG_SerialRemoteParser *self);

#if defined(__cplusplus)
}
#endif
