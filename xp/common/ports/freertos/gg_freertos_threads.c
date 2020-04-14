/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-10-12
 *
 * @details
 *
 * MyNewt implementation of the threads functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Mutex {
    SemaphoreHandle_t handle;
    StaticSemaphore_t memory;
};

struct GG_Semaphore {
    SemaphoreHandle_t handle;
    StaticSemaphore_t memory;
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_FREERTOS_SEMAPHORE_MAX_COUNT 0xFFFF

/*----------------------------------------------------------------------
|   logger
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.threads.freertos");

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Create(GG_Mutex** mutex)
{
    GG_ASSERT(mutex);

    // allocate a new object
    *mutex = GG_AllocateZeroMemory(sizeof(GG_Mutex));
    if (*mutex == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    (*mutex)->handle = xSemaphoreCreateMutexStatic(&(*mutex)->memory);
    if (!(*mutex)->handle) {
        GG_LOG_SEVERE("xSemaphoreCreateMutexStatic failed");
        GG_FreeMemory(*mutex);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Lock(GG_Mutex* self)
{
    GG_ASSERT(self);

    if (xSemaphoreTake(self->handle, portMAX_DELAY) != pdTRUE) {
        GG_LOG_SEVERE("xSemaphoreTake failed");
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Mutex_Unlock(GG_Mutex* self)
{
    GG_ASSERT(self);

    if (xSemaphoreGive(self->handle) != pdTRUE) {
        GG_LOG_SEVERE("xSemaphoreGive failed");
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Mutex_Destroy(GG_Mutex* self)
{
    if (!self) return;

    vSemaphoreDelete(self->handle);
    GG_FreeMemory(self);
}

//----------------------------------------------------------------------
// NOTE: this probably only works on the ESP32 port of FreeRTOS
// and should be revisited for a more general FreeRTOS solution
//----------------------------------------------------------------------
GG_Result
GG_Mutex_LockAutoCreate(GG_Mutex** mutex)
{
    GG_ASSERT(mutex);

    GG_Result result = GG_SUCCESS;

    // create the mutex if it is NULL, in an atomic way
    static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&lock);
    if (*mutex == NULL) {
        result = GG_Mutex_Create(mutex);
    }
    portEXIT_CRITICAL(&lock);

    if (GG_FAILED(result)) {
        return result;
    }

    return GG_Mutex_Lock(*mutex);
}

//----------------------------------------------------------------------
GG_ThreadId
GG_GetCurrentThreadId(void)
{
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    return (GG_ThreadId)current_task;
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
    (*semaphore)->handle = xSemaphoreCreateCountingStatic(GG_FREERTOS_SEMAPHORE_MAX_COUNT,
                                                          initial_value,
                                                          &(*semaphore)->memory);
    if (!(*semaphore)->handle) {
        GG_LOG_SEVERE("xSemaphoreCreateCountingStatic failed");
        GG_FreeMemory(*semaphore);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Semaphore_Acquire(GG_Semaphore* self)
{
    if (xSemaphoreTake(self->handle, portMAX_DELAY) != pdTRUE) {
        GG_LOG_SEVERE("xSemaphoreTake failed");
    }
}

//----------------------------------------------------------------------
void
GG_Semaphore_Release(GG_Semaphore* self)
{
    if (xSemaphoreGive(self->handle) != pdTRUE) {
        GG_LOG_SEVERE("xSemaphoreGive failed");
    }
}

//----------------------------------------------------------------------
void
GG_Semaphore_Destroy(GG_Semaphore* self)
{
    if (!self) return;

    vSemaphoreDelete(self->handle);
    GG_FreeMemory(self);
}
