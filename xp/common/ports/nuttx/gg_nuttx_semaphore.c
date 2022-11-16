
/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2021 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Baktash Boghrati
 *
 * @date 2021-10-25
 *
 * @details
 *
 * NUTTX implementation of the Semaphore functions.
 * The implementations is based on the POSIX port with once difference 
 * in how the semaphores are initialized. NUTTX requires the priority 
 * inheritance to be disabled for signaling semaphores. GG uses 
 * semaphores exclusively for signaling and mutexes for locking. 
 * For more details see:
 * https://cwiki.apache.org/confluence/display/NUTTX/Signaling+Semaphores+and+Priority+Inheritance
 *
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <semaphore.h>
#include <errno.h>

#include <nuttx/semaphore.h>

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
GG_SET_LOCAL_LOGGER("gg.xp.semaphore.nuttx")

//----------------------------------------------------------------------
GG_Result
GG_Semaphore_Create(unsigned int initial_value, GG_Semaphore** semaphore)
{
    // allocate a new object
    *semaphore = GG_AllocateZeroMemory(sizeof(GG_Semaphore));
    if (*semaphore == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    int result;
    int ret = GG_SUCCESS;
    result = sem_init(&(*semaphore)->semaphore, 0, initial_value);
    if (result != 0) {
        GG_LOG_WARNING("sem_init failed (%d)", errno);
        ret = GG_ERROR_ERRNO(errno);
        goto cleanup;
    }

    if (initial_value == 0) { // Check for signaling semaphore
        result = sem_setprotocol(&(*semaphore)->semaphore, SEM_PRIO_NONE);
        if (result != 0) {
            GG_LOG_WARNING("sem_setprotocol failed (%d)", errno);
            ret = GG_ERROR_ERRNO(errno);
            result = sem_destroy(&(*semaphore)->semaphore);
            if (result != 0) {
                GG_LOG_WARNING("sem_destroy failed (%d)", errno);
            }
            goto cleanup;
        }
    }

    return ret;

cleanup:
        GG_FreeMemory(*semaphore);
        *semaphore = NULL;
        return ret;
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
