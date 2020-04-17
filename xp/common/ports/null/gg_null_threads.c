/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author
 *
 * @date 2017-11-09
 *
 * @details
 *
 * dummy implementation of the threads functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_threads.h"
#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Mutex {
    uint32_t mutex;
};

struct GG_Semaphore {
    uint32_t semaphore;
};

//----------------------------------------------------------------------
static GG_Mutex GG_NullMutex = {0};
static GG_Semaphore GG_NullSemaphore = {0};

//----------------------------------------------------------------------
GG_ThreadId
GG_GetCurrentThreadId(void)
{
    return (GG_ThreadId)0;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Create(GG_Mutex** mutex)
{
    *mutex = &GG_NullMutex;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_LockAutoCreate(GG_Mutex** mutex)
{
    *mutex = &GG_NullMutex;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Lock(GG_Mutex* self)
{
    GG_COMPILER_UNUSED(self);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Unlock(GG_Mutex* self)
{
    GG_COMPILER_UNUSED(self);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Mutex_Destroy(GG_Mutex* mutex)
{
    GG_COMPILER_UNUSED(mutex);
}

//----------------------------------------------------------------------
GG_Result
GG_Semaphore_Create(unsigned int initial_value, GG_Semaphore** semaphore)
{
    *semaphore = &GG_NullSemaphore;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Semaphore_Acquire(GG_Semaphore* self)
{
    GG_COMPILER_UNUSED(self);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Release(GG_Semaphore* self)
{
    GG_COMPILER_UNUSED(self);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Destroy(GG_Semaphore* self)
{
    GG_COMPILER_UNUSED(self);
}
