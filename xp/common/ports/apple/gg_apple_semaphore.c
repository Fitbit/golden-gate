/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-01-22
 *
 * @details
 *
 * Apple/GCD implementation of the GG_Semaphore API
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <dispatch/dispatch.h>

#include "xp/common/gg_threads.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Semaphore {
    dispatch_semaphore_t semaphore;
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_Semaphore_Create(unsigned int initial_value, GG_Semaphore** semaphore)
{
    *semaphore = (GG_Semaphore*)GG_AllocateMemory(sizeof(GG_Semaphore));
    if (*semaphore == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }
    (*semaphore)->semaphore = dispatch_semaphore_create((long)initial_value);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Semaphore_Acquire(GG_Semaphore* self)
{
    dispatch_semaphore_wait(self->semaphore, DISPATCH_TIME_FOREVER);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Release(GG_Semaphore* self)
{
    dispatch_semaphore_signal(self->semaphore);
}

//----------------------------------------------------------------------
void
GG_Semaphore_Destroy(GG_Semaphore* self)
{
    dispatch_release(self->semaphore);
}
