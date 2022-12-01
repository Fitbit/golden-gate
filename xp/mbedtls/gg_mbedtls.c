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
 * @date 2019-08-07
 *
 * @details
 *
 * MBEDTLS library initialization.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "xp/common/gg_memory.h"
#include "xp/common/gg_results.h"
#include "xp/module/gg_module.h"

#include "mbedtls/platform.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_MBEDTLS_MAX_CALLOC_COUNT ((uint32_t)0xFFFFFF) // 16M items
#define GG_MBEDTLS_MAX_CALLOC_SIZE  ((uint32_t)0xFFFFFF) // 16MB

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void*
gg_mbedtls_calloc(size_t count, size_t size)
{
    // sanity-check (and avoid overflows)
    if ((count > GG_MBEDTLS_MAX_CALLOC_COUNT || size > GG_MBEDTLS_MAX_CALLOC_SIZE) ||
        (((uint64_t)count * (uint64_t)size) > (uint64_t)GG_MBEDTLS_MAX_CALLOC_SIZE)) {
        return NULL;
    }
    return GG_AllocateZeroMemory(count * size);
}

// we declare a local prototype for this because those init functions don't
// appear in any header file, they are referenced by macro construction
extern GG_Result
GG_MBEDTLS_Init(void);
GG_Result GG_MBEDTLS_Init(void) {
    int result = mbedtls_platform_set_calloc_free(gg_mbedtls_calloc, GG_FreeMemory);

    return result == 0 ? GG_SUCCESS : GG_FAILURE;
}
