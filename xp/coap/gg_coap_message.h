/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 *
 * @details
 *
 * CoAP library - messages (internal header)
 */

#pragma once

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
#include "xp/common/gg_common.h"
#include "xp/coap/gg_coap.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

#if defined(GG_COAP_MESSAGE_PRIVATE)

/**
 * Internal representation of a CoAP message
 * (not visible to the public API)
 */
struct GG_CoapMessage {
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    GG_Buffer*     buffer;         ///< Buffer that contains the encoded message
    const uint8_t* data;           ///< Pointer to the buffer data (convenience shortcut)
    size_t         payload_offset; ///< Offset of the payload portion of the data
    size_t         payload_size;   ///< Size of the payload portion of the data

    GG_THREAD_GUARD_ENABLE_BINDING
};

#endif
