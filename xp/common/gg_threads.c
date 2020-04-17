/**
 * @file
 * @brief Multithreading support
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-02-28
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <inttypes.h>

#include "gg_logging.h"
#include "gg_threads.h"

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static volatile GG_ThreadId GG_ThreadGuard_MainLoopThreadId;

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.thread-guard")

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
void
GG_ThreadGuard_SetMainLoopThreadId(GG_ThreadId thread_id)
{
    GG_ThreadGuard_MainLoopThreadId = thread_id;
}

/*--------------------------------------------------------------------*/
bool
GG_ThreadGuard_CheckCurrentThreadIsMainLoop(const char* caller_name)
{
    if (GG_ThreadGuard_MainLoopThreadId == 0 ||
        GG_ThreadGuard_MainLoopThreadId == GG_GetCurrentThreadId()) {
        GG_COMPILER_UNUSED(caller_name);
        return true;
    } else {
        GG_LOG_SEVERE("current thread isn't the main loop thread [%s]",
                      caller_name ? caller_name : "");
        return false;
    }
}

/*--------------------------------------------------------------------*/
bool
GG_ThreadGuard_CheckCurrentThreadIsExpected(GG_ThreadId expected_thread_id,
                                            const char* caller_name)
{
    GG_ThreadId current_thread_id = GG_GetCurrentThreadId();
    if (current_thread_id == expected_thread_id) {
        GG_COMPILER_UNUSED(caller_name);
        return true;
    } else {
        GG_LOG_SEVERE("current thread (%"PRIu64") doesn't match expected thread (%"PRIu64") [%s]",
                      (uint64_t)current_thread_id,
                      (uint64_t)expected_thread_id,
                      caller_name ? caller_name : "");
        return false;
    }
}
