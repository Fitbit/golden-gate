/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-11-16
 *
 * @details
 *
 * Windows LWIP port.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_system.h"
#include "xp/common/gg_utils.h"

#include "lwip/sys.h"
#include "lwip/opt.h"

/*----------------------------------------------------------------------
|   Return a timestamp in milliseconds
+---------------------------------------------------------------------*/
u32_t
sys_now(void)
{
    // get a timestamp in nanoseconds
    GG_Timestamp now = GG_System_GetCurrentTimestamp();

    // convert to milliseconds
    return (u32_t)(now / GG_NANOSECONDS_PER_MILLISECOND);
}
