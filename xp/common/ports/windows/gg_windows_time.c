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
 * @date 2017-12-26
 *
 * @details
 *
 * Windows implementation of the system time interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <windows.h>

#include "xp/common/gg_system.h"
#include "xp/common/gg_utils.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Timestamp
GG_System_GetCurrentTimestamp(void)
{
    static uint64_t timer_frequency = 0;
    if (timer_frequency == 0) {
        // initialize the frequency
        LARGE_INTEGER _timer_frequency;
        if (QueryPerformanceFrequency(&_timer_frequency)) {
            timer_frequency = (uint64_t)_timer_frequency.QuadPart;
        }
    }

    // get the current timer
    LARGE_INTEGER now;
    if (timer_frequency && QueryPerformanceCounter(&now)) {
        return (GG_NANOSECONDS_PER_SECOND * (uint64_t)now.QuadPart) / timer_frequency;
    } else {
        // something went wrong
        return 0;
    }
}
