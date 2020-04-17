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
 * @date 2018-01-22
 *
 * @details
 *
 * POSIX implementation of the GG_Mutex API
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <pthread.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Mutex {
    pthread_mutex_t mutex;
};

/*----------------------------------------------------------------------
|   logger
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.mutex.posix")

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Create(GG_Mutex** mutex)
{
    GG_ASSERT(mutex);

    // allocate a new object
    *mutex = GG_AllocateZeroMemory(sizeof(GG_Mutex));
    if (*mutex == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    int result = pthread_mutex_init(&(*mutex)->mutex, NULL);
    if (result != 0) {
        GG_LOG_SEVERE("pthread mutex init failed with error %d", (int)result);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_LockAutoCreate(GG_Mutex** mutex)
{
    GG_ASSERT(mutex);
    GG_Result result = GG_SUCCESS;

    // protect this mutex creation with a static shared outer lock
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&lock);

    // create the mutex if it doesn't already exist
    if (*mutex == NULL) {
        result = GG_Mutex_Create(mutex);
    }

    pthread_mutex_unlock(&lock);

    // check that the mutex could be created
    if (GG_FAILED(result)) {
        return result;
    }

    // lock the mutex
    int pthread_result = pthread_mutex_lock(&(*mutex)->mutex);
    if (pthread_result != 0) {
        GG_LOG_SEVERE("pthread mutex lock failed with error %d", pthread_result);
        result = GG_FAILURE;
    }

    return result;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Lock(GG_Mutex* mutex)
{
    GG_ASSERT(mutex);

    int result = pthread_mutex_lock(&mutex->mutex);
    if (result != 0) {
        GG_LOG_SEVERE("pthread mutex lock failed with error %d", result);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Unlock(GG_Mutex* mutex)
{
    GG_ASSERT(mutex);

    int result = pthread_mutex_unlock(&mutex->mutex);
    if (result != 0) {
        GG_LOG_SEVERE("pthread mutex unlock failed with error %d", result);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Mutex_Destroy(GG_Mutex* mutex)
{
    if (mutex) {
        pthread_mutex_destroy(&mutex->mutex);
        GG_ClearAndFreeObject(mutex, 0);
    }
}
