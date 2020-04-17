/**
 * @file
 * @brief General purpose memory allocation and management functions
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stddef.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/*
 * Callback function for reporting memory allocation failures
 *
 * @param size Size for which memory allocation failed
 */
typedef void (*GG_AllocateMemoryFailureCallback)(size_t size);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//! @addtogroup Memory Memory Management
//! Memory management.
//! @{

/*----------------------------------------------------------------------
|   heap allocation
+---------------------------------------------------------------------*/
/**
 * Allocate memory from the default application heap.
 * The memory is not initialized.
 *
 * @param size Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL when out of memory.
 */
void* GG_AllocateMemory(size_t size);

/**
 * Allocate memory from the default application heap and initialize it to zeros.
 *
 * @param size Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL when out of memory.
 */
void* GG_AllocateZeroMemory(size_t size);

/**
 * Free a block of memory that was allocated from the default application heap.
 *
 * @param memory The memory to free.
 */
void GG_FreeMemory(void* memory);

/**
 * Clear a block of memory that was allocated from the default application heap, then free it.
 * Clearing fills the memory with 0s.
 * Optionally, this function can also set one or more vtable "traps" at the start of the memory
 * block, which allows catching invalid calls to virtual functions on objects that have been free'd.
 *
 * @param memory The memory to clear and free.
 * @param memory_size The size of the memory block to clear.
 * @param trap_count The number of vtable pointers to set a trap for at the start of the block.
 */
void GG_ClearAndFreeMemory(void* memory, size_t memory_size, size_t trap_count);

/**
 * Convenience macro to call GG_ClearAndFreeMemory with the memory size set to the size of
 * the object being free'd.
 */
#define GG_ClearAndFreeObject(_object, _trap_count) GG_ClearAndFreeMemory((_object), sizeof(*(_object)), (_trap_count))

/**
 * Wrap a function invocation inside an autorelease context.
 * (Needed by some platforms to avoid long term retention of objects)
 *
 * @param function Pointer to the function that should be invoked from within the wrapping context.
 * @param arg Argument that will be passed to the invoked function.
 *
 * @return The result returned by the wrapped function.
 */
GG_Result GG_AutoreleaseWrap(GG_Result (*function)(void* arg), void* arg);

/**
 * Register callback to be called when an GG allocate memory call fails
 *
 * The callback can be register to call platform specific code when a memory
 * allocation fails within the GG lib.
 *
 * @param callback Pointer to function to be called on memory allocation failure
 */
void GG_RegisterAllocateMemoryFailureCallback(GG_AllocateMemoryFailureCallback callback);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
