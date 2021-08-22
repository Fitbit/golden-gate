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

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_AllocateMemoryFailureCallback failure_callback;

// When memory stats is enabled, the allocator will keep track of memory
// allocations and also log periodically some of the stats.
// NOTE: this has a (small) performance hit, and uses an additional sizeof(size_t)
// bytes per allocated memory block (used to record the block size)
#if defined(GG_CONFIG_ENABLE_MEMORY_STATS)
#include <string.h> // for memset()
#include "xp/common/gg_logging.h"
#include "xp/common/gg_threads.h"
GG_SET_LOCAL_LOGGER("gg.xp.memory.stdc")

/** Configurable interval for loggig memory stats */
#if !defined(GG_CONFIG_MEMORY_STATS_LOG_INTERVAL)
#define GG_CONFIG_MEMORY_STATS_LOG_INTERVAL 1024 // Log once very 1024 allocations
#endif

/** Configurable threshold to log large block allocations */
#if !defined(GG_CONFIG_MEMORY_STATS_LOG_LARGE_CHUNK_THRESHOLD)
#define GG_CONFIG_MEMORY_STATS_LOG_LARGE_CHUNK_THRESHOLD 65536
#endif

static struct {
    size_t allocations_count; ///< Total number of allocations
    size_t allocation_max;    ///< Size of the largest block allocated
    size_t block_count;       ///< Number of allocated blocks currently
    size_t block_count_max;   ///< Maximum number of allocated blocks at any time
    size_t heap_used;         ///< Current heap used
    size_t heap_used_max;     ///< Maximum heap used
    bool   should_log;        ///< Indicates that the state should be logged
} g_memory_stats;

static GG_Mutex* g_memory_stats_lock;

static bool g_memory_stats_initialized;

#endif

/*--------------------------------------------------------------------*/
GG_Result
GG_Memory_Initialize(void)
{
#if defined(GG_CONFIG_ENABLE_MEMORY_STATS)
    if (g_memory_stats_initialized) {
        return GG_SUCCESS;
    }
    GG_Result result = GG_Mutex_Create(&g_memory_stats_lock);
    if (GG_SUCCEEDED(result)) {
        g_memory_stats_initialized = true;
    }
    return result;
#else
    return GG_SUCCESS;
#endif
}

/*--------------------------------------------------------------------*/
#if defined(GG_CONFIG_ENABLE_MEMORY_STATS)
static void
GG_LogMemoryStats(void) {
    GG_LOG_FINE("mem stats: count=%zu, max_size=%zu, blocks=%zu, max_blocks=%zu, used=%zu, max_used=%zu",
                g_memory_stats.allocations_count,
                g_memory_stats.allocation_max,
                g_memory_stats.block_count,
                g_memory_stats.block_count_max,
                g_memory_stats.heap_used,
                g_memory_stats.heap_used_max);
}
#endif

/*--------------------------------------------------------------------*/
void*
GG_AllocateMemory(size_t size)
{
    void* memory = malloc(size
#if defined(GG_CONFIG_ENABLE_MEMORY_STATS)
        + sizeof(size_t)); // Add some overhead to keep track of the size

    if (size > GG_CONFIG_MEMORY_STATS_LOG_LARGE_CHUNK_THRESHOLD) {
        GG_LOG_WARNING("large block allocation: %zu", size);
    }

    if (memory) {
        size_t* block = (size_t*)memory;
        *block = size;   // Remember the size
        memory = block + 1; // Return the memory past the size field

        if (g_memory_stats_initialized && g_memory_stats_lock) {
            GG_Result result = GG_Mutex_Lock(g_memory_stats_lock);
            if (GG_SUCCEEDED(result)) {
                ++g_memory_stats.allocations_count;
                if (size > g_memory_stats.allocation_max) {
                    g_memory_stats.allocation_max = size;
                }
                g_memory_stats.heap_used += size; // Don't count the overhead
                if (g_memory_stats.heap_used > g_memory_stats.heap_used_max) {
                    g_memory_stats.heap_used_max = g_memory_stats.heap_used;
                }
                if (++g_memory_stats.block_count > g_memory_stats.block_count_max) {
                    g_memory_stats.block_count_max = g_memory_stats.block_count;
                }

                if (g_memory_stats.allocations_count % GG_CONFIG_MEMORY_STATS_LOG_INTERVAL == 0) {
                    g_memory_stats.should_log = true;
                }
                GG_Mutex_Unlock(g_memory_stats_lock);
            }
        }
    }
#else
    );
#endif

    if (memory == NULL && failure_callback) {
        failure_callback(size);
    }

    return memory;
}

/*--------------------------------------------------------------------*/
void*
GG_AllocateZeroMemory(size_t size)
{
#if defined(GG_CONFIG_ENABLE_MEMORY_STATS)
    void* memory = GG_AllocateMemory(size);
    if (memory) {
        memset(memory, 0, size);
    }
#else
    void* memory = calloc(1, size);
#endif

    if (memory == NULL && failure_callback) {
        failure_callback(size);
    }

    return memory;
}

/*--------------------------------------------------------------------*/
void
GG_FreeMemory(void* memory)
{
#if defined(GG_CONFIG_ENABLE_MEMORY_STATS)
    if (memory != NULL) {
        size_t* block = (size_t*)memory - 1;
        size_t block_size = *block;
        if (block_size > g_memory_stats.allocation_max ||
            block_size > g_memory_stats.heap_used) {
            // Something's not right here
            GG_LOG_SEVERE("bogus block size %zu for pointer %p", block_size, memory);
            GG_LogMemoryStats();
            assert(0);
        } else {
            *block = (size_t)-1; // Set a trap in case this gets free'd again
            if (g_memory_stats_initialized && g_memory_stats_lock) {
                GG_Result result = GG_Mutex_Lock(g_memory_stats_lock);
                if (GG_SUCCEEDED(result)) {
                    if (g_memory_stats.block_count) {
                        --g_memory_stats.block_count;
                    } else {
                        GG_LOG_SEVERE("inconsistent block count: block free'd with 0 blocks allocated");
                    }
                    g_memory_stats.heap_used -= block_size;
                    GG_Mutex_Unlock(g_memory_stats_lock);
                }
            }
        }
        memory = block;
    }
    
    // Check if the stats are ready to be logged.
    // NOTE: we choose to log here because since logging may allocate memory, if we chose to log
    // when memory is allocated, it would re-enter, causing unnecessary recursion.
    // Also, we are not looking at this state with the lock held, but that's Ok because stats
    // logging isn't supposed to be done precisely at the specified interval, which is just an
    // approximation.
    if (g_memory_stats.should_log) {
        GG_LogMemoryStats();
        g_memory_stats.should_log = false;
    }
#endif
    free(memory);
}

/*--------------------------------------------------------------------*/
void
GG_RegisterAllocateMemoryFailureCallback(GG_AllocateMemoryFailureCallback callback)
{
    failure_callback = callback;
}
