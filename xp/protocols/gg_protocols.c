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
 * @date 2017-11-21
 *
 * @details
 * Misc protocol parsers and helpers
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_protocols.h"

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_FrameAssembler_GetFeedBuffer(GG_FrameAssembler* self, uint8_t** buffer, size_t* buffer_size)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->GetFeedBuffer(self, buffer, buffer_size);
}

//----------------------------------------------------------------------
GG_Result
GG_FrameAssembler_Feed(GG_FrameAssembler* self, size_t* data_size, GG_Buffer**frame)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->Feed(self, data_size, frame);
}

//----------------------------------------------------------------------
void
GG_FrameAssembler_Reset(GG_FrameAssembler* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->Reset(self);
}

//----------------------------------------------------------------------
GG_Result
GG_FrameSerializer_SerializeFrame(GG_FrameSerializer* self,
                                  const uint8_t*      frame,
                                  size_t              frame_size,
                                  GG_RingBuffer*      output)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->SerializeFrame(self, frame, frame_size, output);
}
