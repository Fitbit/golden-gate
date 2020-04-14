/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Bogdan Davidoaia
 *
 * @date 2017-11-13
 *
 * @details
 *
 * MyNewt implementation of the threads functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "os/os.h"
#include "os/os_mutex.h"
#include "os/os_sem.h"
#include "os/os_task.h"

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Mutex {
    struct os_mutex mutex;
};

struct GG_Semaphore {
    struct os_sem semaphore;
};

/*----------------------------------------------------------------------
|   logger
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.threads.mynewt");

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Create(GG_Mutex** mutex)
{
    os_error_t result;

    GG_ASSERT(mutex);

    // allocate a new object
    *mutex = GG_AllocateZeroMemory(sizeof(GG_Mutex));
    if (*mutex == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    result = os_mutex_init(&(*mutex)->mutex);
    if (result != OS_OK) {
        GG_LOG_SEVERE("mynewt mutex init failed with error %d", (int)result);
        GG_FreeMemory(*mutex);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Lock(GG_Mutex* mutex)
{
    os_error_t result;

    GG_ASSERT(mutex);

    result = os_mutex_pend(&mutex->mutex, OS_TIMEOUT_NEVER);
    if (result != 0) {
        GG_LOG_SEVERE("mynewt mutex lock failed with error %d", result);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Unlock(GG_Mutex* mutex)
{
    GG_ASSERT(mutex);

    int result = os_mutex_release(&mutex->mutex);
    if (result != 0) {
        GG_LOG_SEVERE("mynewt mutex unlock failed with error %d", result);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Mutex_Destroy(GG_Mutex* mutex)
{
    if (mutex) {
        GG_FreeMemory(mutex);
    }
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_LockAutoCreate(GG_Mutex** mutex)
{
    GG_ASSERT(mutex);

    GG_Result result = GG_SUCCESS;

    // create the mutex if it is NULL, in an atomic way
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    if (*mutex == NULL) {
        result = GG_Mutex_Create(mutex);
    }
    OS_EXIT_CRITICAL(sr);

    if (GG_FAILED(result)) {
        return result;
    }

    return GG_Mutex_Lock(*mutex);
}

//----------------------------------------------------------------------
GG_ThreadId
GG_GetCurrentThreadId(void)
{
    struct os_task *task;

    task = os_sched_get_current_task();

    return (GG_ThreadId)task->t_taskid;
}

//----------------------------------------------------------------------
GG_Result
GG_Semaphore_Create(unsigned int initial_value, GG_Semaphore** semaphore)
{
    // check that we can represent this initial value, just in case
    if (initial_value > UINT16_MAX) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // allocate a new object
    *semaphore = GG_AllocateMemory(sizeof(GG_Semaphore));
    if (*semaphore == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }
    os_error_t result = os_sem_init(&(*semaphore)->semaphore, (uint16_t)initial_value);
    if (result != OS_OK) {
        GG_LOG_SEVERE("os_sem_init failed with error %d", (int)result);
        GG_FreeMemory(*semaphore);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Semaphore_Acquire(GG_Semaphore* self)
{
    os_sem_pend(&self->semaphore, OS_TIMEOUT_NEVER);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Release(GG_Semaphore* self)
{
    os_sem_release(&self->semaphore);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Destroy(GG_Semaphore* self)
{
    if (self) {
        GG_FreeMemory(self);
    }
}
