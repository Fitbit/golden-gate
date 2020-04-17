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
 * @date 2018-03-06
 *
 * @details
 *
 * Standard C implementation of the random API
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"

//----------------------------------------------------------------------
uint32_t
GG_GetRandomInteger(void)
{
    // get two 16-bit random values (because the rand function doesn't always return 32 bits)
    uint32_t r1 = (uint32_t)(rand() & 0xFFFF);
    uint32_t r2 = (uint32_t)(rand() & 0xFFFF);

    return (r1 << 16) | r2;
}

//----------------------------------------------------------------------
void
GG_GetRandomBytes(uint8_t* buffer, size_t buffer_size)
{
    for (size_t i = 0; i < buffer_size; i++) {
        buffer[i] = (uint8_t)rand();
    }
}
