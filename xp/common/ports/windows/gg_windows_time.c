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
    static double timer_frequency = 0.0; // timer frequency in units per nanoseconds
    if (timer_frequency == 0.0) {
        // initialize the frequency
        LARGE_INTEGER _timer_frequency;
        if (QueryPerformanceFrequency(&_timer_frequency)) {
            timer_frequency = (double)_timer_frequency.QuadPart / (double)GG_NANOSECONDS_PER_SECOND;
        }
    }

    // get the current timer
    LARGE_INTEGER now;
    if (timer_frequency && QueryPerformanceCounter(&now)) {
        return (GG_Timestamp)((double)now.QuadPart / timer_frequency);
    } else {
        // something went wrong
        return 0;
    }
}
