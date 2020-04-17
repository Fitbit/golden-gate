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
 * @date 2017-11-16
 *
 * @details
 *
 * Android LWIP port.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/stats.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
get_monotonic_time(struct timespec *ts)
{
  clock_gettime(CLOCK_MONOTONIC, ts);
}


/*----------------------------------------------------------------------
|   Return a timestamp in milliseconds
+---------------------------------------------------------------------*/
u32_t
sys_now(void)
{
    struct timespec ts;

    get_monotonic_time(&ts);
    return (u32_t)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}
