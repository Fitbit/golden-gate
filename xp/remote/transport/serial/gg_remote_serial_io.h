/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author
 *
 * @date 2017-11-07
 *
 * @details
 *
 * Remote Serial IO
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
#include "gg_remote_parser.h"

/*----------------------------------------------------------------------
|   GG_SerialIO interface
+---------------------------------------------------------------------*/
GG_DECLARE_INTERFACE(GG_SerialIO) {
    GG_Result (*ReadFrame)(GG_SerialIO* self, GG_Buffer** buffer);
    GG_Result (*ReadAck)(GG_SerialIO* self);
    GG_Result (*Write)(GG_SerialIO* self, GG_Buffer* buffer);
};

// thunks
GG_Result GG_SerialIO_ReadFrame(GG_SerialIO* self, GG_Buffer** buffer);
GG_Result GG_SerialIO_ReadAck(GG_SerialIO* self);
GG_Result GG_SerialIO_Write(GG_SerialIO* self, GG_Buffer* buffer);

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_SerialIO);
    GG_SerialRemoteParser* parser;
} SerialIO;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
void SerialIO_Init(SerialIO* serial, GG_SerialRemoteParser* parser);
void SerialIO_Destroy(SerialIO* serial);
