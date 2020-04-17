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

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "xp/common/gg_port.h"
#include "gg_remote_serial_io.h"

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result GG_SerialIO_ReadFrame(GG_SerialIO* self, GG_Buffer** buffer) {
    GG_ASSERT(self);
    return GG_INTERFACE(self)->ReadFrame(self, buffer);
}

//----------------------------------------------------------------------
GG_Result GG_SerialIO_ReadAck(GG_SerialIO* self) {
    GG_ASSERT(self);
    return GG_INTERFACE(self)->ReadAck(self);
}

//----------------------------------------------------------------------
GG_Result GG_SerialIO_Write(GG_SerialIO* self, GG_Buffer* buffer) {
    GG_ASSERT(self);
    return GG_INTERFACE(self)->Write(self, buffer);
}
