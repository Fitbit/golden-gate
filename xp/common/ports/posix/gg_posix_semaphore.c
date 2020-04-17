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
 * POSIX implementation of the threads functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <semaphore.h>
#include <errno.h>

#include "xp/common/gg_memory.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_logging.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Semaphore {
    sem_t semaphore;
};

/*----------------------------------------------------------------------
|   logger
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.semaphore.posix")

//----------------------------------------------------------------------
GG_Result
GG_Semaphore_Create(unsigned int initial_value, GG_Semaphore** semaphore)
{
    // allocate a new object
    *semaphore = GG_AllocateZeroMemory(sizeof(GG_Semaphore));
    if (*semaphore == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    int result = sem_init(&(*semaphore)->semaphore, 0, initial_value);
    if (result != 0) {
        GG_LOG_WARNING("sem_init failed (%d)", errno);
        GG_FreeMemory(*semaphore);
        *semaphore = NULL;
        return GG_ERROR_ERRNO(errno);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Semaphore_Acquire(GG_Semaphore* self)
{
    for (;;) {
        int result = sem_wait(&self->semaphore);
        if (result != 0) {
            if (errno == EINTR) {
                GG_LOG_FINE("sem_wait was interrupted, retrying");
                continue;
            } else {
                GG_LOG_SEVERE("sem_wait returned %d", result);
            }
        }
        break;
    }
}

//----------------------------------------------------------------------
void
GG_Semaphore_Release(GG_Semaphore* self)
{
    sem_post(&self->semaphore);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Destroy(GG_Semaphore* self)
{
    if (!self) return;

    sem_destroy(&self->semaphore);
    GG_FreeMemory(self);
}
