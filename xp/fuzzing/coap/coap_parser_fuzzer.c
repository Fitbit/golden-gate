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
    // copy the input data to a buffer object
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(size, &buffer);
    if (GG_FAILED(result)) {
        return 0;
    }
    result = GG_DynamicBuffer_SetData(buffer, data, size);
    if (GG_FAILED(result)) {
        return 0;
    }

    // parse the buffer
    GG_CoapMessage* message = NULL;
    result = GG_CoapMessage_CreateFromDatagram(GG_DynamicBuffer_AsBuffer(buffer), &message);
    if (GG_SUCCEEDED(result)) {
        // re-encode it to a datagram
        GG_Buffer* output = NULL;
        result = GG_CoapMessage_ToDatagram(message, &output);
        if (GG_SUCCEEDED(result)) {
            GG_Buffer_Release(output);
        }

        // iterate all options
        GG_CoapMessageOptionIterator iterator;
        GG_CoapMessage_InitOptionIterator(message, GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY, &iterator);
        while (iterator.option.number != GG_COAP_MESSAGE_OPTION_NONE) {
            GG_CoapMessage_StepOptionIterator(message, &iterator);
        }

        GG_CoapMessage_Destroy(message);
    }


    // cleanup
    GG_DynamicBuffer_Release(buffer);

    return 0;
}
