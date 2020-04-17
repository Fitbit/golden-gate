/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Sreejith Unnikrishnan
 *
 * @date 2017-11-09
 *
 * @details
 *
 * This file defines a GG stdlib allocator for Fb-SMO.
 */

#include <stdlib.h>
#include <string.h>
#include "fb_smo.h"
#include "gg_smo_allocator.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void*
GG_SmoHeapAllocator_allocate_memory(Fb_SmoAllocator* self, size_t size)
{
    GG_COMPILER_UNUSED(self);
    return GG_AllocateMemory(size);
}

//---------------------------------------------------------------------
static void
GG_SmoHeapAllocator_free_memory(Fb_SmoAllocator* self, void* memory)
{
    GG_COMPILER_UNUSED(self);
    GG_FreeMemory(memory);
}

//---------------------------------------------------------------------
Fb_SmoAllocator
GG_SmoHeapAllocator = {
    GG_SmoHeapAllocator_allocate_memory,
    GG_SmoHeapAllocator_free_memory
};
