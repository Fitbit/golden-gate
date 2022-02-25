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

#include "fb_smo.h"
#include "fb_smo_serialization.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   fuzzer target entry point
+---------------------------------------------------------------------*/
int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Fb_Smo* smo = NULL;
    int result = Fb_Smo_Deserialize(NULL,
                                    NULL,
                                    FB_SMO_SERIALIZATION_FORMAT_CBOR,
                                    data,
                                    (unsigned int)size,
                                    &smo);
    if (result != FB_SMO_SUCCESS) {
        return 0;
    }

    // re-serialize
    unsigned int serialized_size;
    result = Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, NULL, &serialized_size);
    if (result == FB_SMO_SUCCESS && serialized_size) {
        uint8_t* buffer = GG_AllocateMemory(serialized_size);
        if (buffer) {
            Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, buffer, &serialized_size);
            GG_FreeMemory(buffer);
        }
    }

    // cleanup
    Fb_Smo_Destroy(smo);

    return 0;
}
