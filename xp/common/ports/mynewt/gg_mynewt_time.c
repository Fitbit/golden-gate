/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Bogdan Davidoaia
 *
 * @date 2017-11-13
 *
 * @details
 *
 * Mynewt implementation of the system time interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "os/os_time.h"

#include "xp/common/gg_system.h"
#include "xp/common/gg_utils.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Timestamp
GG_System_GetCurrentTimestamp(void)
{
    uint64_t now_usec = os_get_uptime_usec();
    return now_usec * (uint64_t)GG_NANOSECONDS_PER_MICROSECOND;
}
