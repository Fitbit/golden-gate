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
 * POSIX implementation of the system time interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <time.h>

#include "xp/common/gg_system.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

#ifdef __MACH__
#include <mach/mach_time.h>

static int clock_gettime_alternate(int clk_id, struct timespec *t) {
    (void)clk_id;
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        // Cache timebase since it's expensive to calculate
        mach_timebase_info(&timebase);
    }
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec  = (time_t)seconds;
    t->tv_nsec = (long)nseconds;
    return 0;
}
#endif

GG_Timestamp
GG_System_GetCurrentTimestamp(void)
{
    struct timespec ts;
    if (__builtin_available(macOS 10.12, iOS 10.0, *)) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
    } else {
        // Fallback on earlier versions
        clock_gettime_alternate(0, &ts);
    }
    return ((uint64_t)ts.tv_sec * GG_NANOSECONDS_PER_SECOND) + ts.tv_nsec;
}
