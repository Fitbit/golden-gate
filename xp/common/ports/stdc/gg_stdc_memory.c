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
 * Implementation of the memory functions
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "xp/common/gg_memory.h"

static GG_AllocateMemoryFailureCallback failure_callback;

/*----------------------------------------------------------------------
|   heap allocation
+---------------------------------------------------------------------*/
void*
GG_AllocateMemory(size_t size)
{
    void* ret = malloc(size);

    if (ret == NULL && failure_callback) {
        failure_callback(size);
    }

    return ret;
}

void*
GG_AllocateZeroMemory(size_t size)
{
    void* ret = calloc(1, size);

    if (ret == NULL && failure_callback) {
        failure_callback(size);
    }

    return ret;
}

void
GG_FreeMemory(void* memory)
{
    free(memory);
}

void
GG_RegisterAllocateMemoryFailureCallback(GG_AllocateMemoryFailureCallback callback)
{
    failure_callback = callback;
}
