/**
 *
 * @file
 *
 * @copyright
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#include "xp/coap/gg_coap.h"

/*----------------------------------------------------------------------
|   fuzzer target entry point
+---------------------------------------------------------------------*/
int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // parse the extended error
    GG_CoapExtendedError extended_error = {0};
    GG_Result result = GG_CoapExtendedError_Decode(&extended_error, data, size);
    if (GG_FAILED(result)) {
        return 0;
    }

    // don't continue unless the namespace or message have 0 length, which can't work when
    // coming from a parsed buffer, because for the encoding API, a 0 value for the string
    // length means that the encoder should measure the length itself, which requires a
    // NULL-terminated string)
    if (!extended_error.name_space_size || !extended_error.message_size) {
        return 0;
    }

    // measure the size
    size_t extended_error_size = GG_CoapExtendedError_GetEncodedSize(&extended_error);

    // re-encode it
    if (extended_error_size) {
        uint8_t* buffer = GG_AllocateMemory(extended_error_size);
        if (buffer == NULL) {
            return 0;
        }

        GG_CoapExtendedError_Encode(&extended_error, buffer);

        // cleanup
        GG_FreeMemory(buffer);
    }

    return 0;
}
