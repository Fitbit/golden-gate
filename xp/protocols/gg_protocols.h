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
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup ProtocolHelpers Protocol Helpers
//! Interfaces and Classes that are used to help implement protocols.
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_common.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
//! Interface implemented by frame assemblers.
//! A frame assembler is an object that can re-assemble a stream of bytes
//! into a frame, typically for the purpose of re-assembling packets in
//! packet-oriented protocols, when the data is received over a transport
//! channel that doesn't already provide support for framing.
//!
//! @interface GG_FrameAssembler
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_FrameAssembler) {
    /**
     * Get the address and size of the buffer into which data can be fed.
     * After this function returns, the caller may write up to *buffer_size
     * bytes into the returned buffer before calling #GG_FrameAssembler_Feed
     *
     * @param self The object on which this method is called.
     * @param[out] buffer Pointer to where the buffer pointer will be returned.
     * @param[out] buffer_size Pointer to where the buffer size will be returned.
     */
    void (*GetFeedBuffer)(GG_FrameAssembler* self, uint8_t** buffer, size_t* buffer_size);

    /**
     * Notify the assembler that data has been copied into the feed buffer, and
     * possibly produce a re-assembled frame.
     * If a frame is produced, it must eventually be released by the caller when no
     * longer needed.
     *
     * @param self The object on which this method is called.
     * @param[in,out] data_size Pointer to a variable containing, when this method is called,
     * the amount of data fed, and, when the method returns, the number of bytes
     * consumed by the frame assembler (always <= the amount passed in).
     * @param[out] frame Pointer to the address of a variable where an assembled frames
     * may be returned, if a complete frame was found.
     *
     * @return #GG_SUCCESS if the method succeeded, or a negative error code.
     */
    GG_Result (*Feed)(GG_FrameAssembler* self, size_t* data_size, GG_Buffer** frame);

    /**
     * Reset the state of the frame assembler.
     *
     * @param self The object on which this method is called.
     */
    void (*Reset)(GG_FrameAssembler* self);
};

//! @var GG_FrameAssembler::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_FrameAssemblerInterface
//! Virtual function table for the GG_FrameAssembler interface

//! @relates GG_FrameAssembler
//! @copydoc GG_FrameAssemblerInterface::GetFeedBuffer
void GG_FrameAssembler_GetFeedBuffer(GG_FrameAssembler* self, uint8_t** buffer, size_t* buffer_size);

//! @relates GG_FrameAssembler
//! @copydoc GG_FrameAssemblerInterface::Feed
GG_Result GG_FrameAssembler_Feed(GG_FrameAssembler* self, size_t* data_size, GG_Buffer** frame);

//! @relates GG_FrameAssembler
//! @copydoc GG_FrameAssemblerInterface::Reset
void GG_FrameAssembler_Reset(GG_FrameAssembler* self);

//----------------------------------------------------------------------
//! Interface implemented by frame serializers.
//! A frame serializer is an object that takes an IP frame and serializes
//! it in way that is compatible with what a GG_FrameAssembler object on
//! the other end of a link can reassemble into a discrete IP packet.
//!
//! @interface GG_FrameSerializer
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_FrameSerializer) {
    /**
     * Serialize a frame into an output buffer.
     * The serializer must consume the entire frame in a single call.
     *
     * @param self The object on which this method is called.
     * @param frame Pointer to the frame data to serialize
     * @param frame_size Size of frame
     * @param output The buffer in which the serialized data should be written.
     *
     * @return GG_SUCCESS if the frame could be serialized, or a negative error code
     */
    GG_Result (*SerializeFrame)(GG_FrameSerializer* self,
                                const uint8_t*      frame,
                                size_t              frame_size,
                                GG_RingBuffer*      output);
};

//! @var GG_FrameSerializer::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_FrameSerializerInterface
//! Virtual function table for the GG_FrameSerializer interface

//! @relates GG_FrameSerializer
//! @copydoc GG_FrameSerializerInterface::SerializeFrame
GG_Result GG_FrameSerializer_SerializeFrame(GG_FrameSerializer* self,
                                            const uint8_t*      frame,
                                            size_t              frame_size,
                                            GG_RingBuffer*      output);

//!@}

#if defined(__cplusplus)
}
#endif
