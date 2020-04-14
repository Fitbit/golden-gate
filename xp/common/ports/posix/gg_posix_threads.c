/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
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
#include <pthread.h>

#include "xp/common/gg_threads.h"

//----------------------------------------------------------------------
GG_ThreadId
GG_GetCurrentThreadId(void)
{
    pthread_t pid = pthread_self();
    return (GG_ThreadId)(pid);
}

