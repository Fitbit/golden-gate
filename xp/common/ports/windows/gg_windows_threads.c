/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-26
 *
 * @details
 *
 * Windows implementation of the threads functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <windows.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.threads.windows")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Mutex {
    CRITICAL_SECTION critical_section;
};

struct GG_Semaphore {
    HANDLE semaphore;
};

//----------------------------------------------------------------------
GG_ThreadId
GG_GetCurrentThreadId(void)
{
    return (GG_ThreadId)GetCurrentThreadId();
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Create(GG_Mutex** mutex)
{
    GG_ASSERT(mutex);

    // allocate a new object
    *mutex = GG_AllocateZeroMemory(sizeof(GG_Mutex));
    if (*mutex == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    InitializeCriticalSection(&(*mutex)->critical_section);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_LockAutoCreate(GG_Mutex** mutex)
{
    GG_ASSERT(mutex);

    if (*mutex == NULL) {
        // create a new mutex
        GG_Mutex* new_mutex;
        GG_Result result = GG_Mutex_Create(&new_mutex);
        if (GG_FAILED(result)) {
            return result;
        }

        // try to set the return value atomically
        void* old_mutex = InterlockedCompareExchangePointer((void**)mutex,
                                                            (void*)new_mutex,
                                                            NULL);
        if (old_mutex != NULL) {
            // *mutex was not NULL: no exchange has been performed
            GG_Mutex_Destroy(new_mutex);
        }
    }

    // lock
    GG_ASSERT(*mutex);
    EnterCriticalSection(&(*mutex)->critical_section);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Lock(GG_Mutex* mutex)
{
    GG_ASSERT(mutex);

    EnterCriticalSection(&mutex->critical_section);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Unlock(GG_Mutex* mutex)
{
    GG_ASSERT(mutex);

    LeaveCriticalSection(&mutex->critical_section);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Mutex_Destroy(GG_Mutex* mutex)
{
    if (!mutex) {
        return;
    }

    DeleteCriticalSection(&mutex->critical_section);
    GG_FreeMemory(mutex);
}

//----------------------------------------------------------------------
GG_Result
GG_Semaphore_Create(unsigned int initial_value, GG_Semaphore** semaphore)
{
    // allocate a new object
    *semaphore = GG_AllocateMemory(sizeof(GG_Semaphore));
    if (*semaphore == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    (*semaphore)->semaphore = CreateSemaphore(NULL,                // default security attributes
                                              (LONG)initial_value, // initial count
                                              LONG_MAX,            // maximum count
                                              NULL);               // unnamed semaphore
    if ((*semaphore)->semaphore == NULL) {
        GG_LOG_WARNING("CreateSemaphore failed (%x)", (int)GetLastError());
        GG_FreeMemory(*semaphore);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Semaphore_Acquire(GG_Semaphore* self)
{
    WaitForSingleObject(self->semaphore, INFINITE);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Release(GG_Semaphore* self)
{
    ReleaseSemaphore(self->semaphore, 1, NULL);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Destroy(GG_Semaphore* self)
{
    if (!self) return;

    CloseHandle(self->semaphore);
    GG_FreeMemory(self);
}
