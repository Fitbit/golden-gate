/**
 * @file
 *
 * @copyright
 * Copyright 2017-2019 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-11-25
 *
 * @details
 *
 * Memory management utilities
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_memory.h"
#include "gg_types.h"
#include "gg_port.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_ClearAndFreeMemory(void* memory, size_t memory_size, size_t trap_count)
{
    // clear the memory
    memset(memory, 0, memory_size);

    // set the traps
    const GG_GenericInterfaceTrapMethod** interfaces = memory;
    for (size_t i = 0; i < trap_count && i < memory_size / sizeof(interfaces[0]); i++) {
        interfaces[i] = GG_GenericInterfaceTrapVTable;
    }

    // free the memory
    GG_FreeMemory(memory);
}
