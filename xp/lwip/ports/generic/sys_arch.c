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
 * Generic LWIP port.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdarg.h>

#include "xp/common/gg_system.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_logging.h"

#include "lwip/sys.h"
#include "lwip/opt.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.lwip.generic")

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

/*----------------------------------------------------------------------
|   Output through the GG logging subsystem
+---------------------------------------------------------------------*/
void GG_LwipPlatformDiag(const char* format, ...)
{
    // format in a local buffer
    va_list ap;
    va_start(ap, format);
    char buffer[64];
    int char_count = vsnprintf(buffer, sizeof(buffer), format, ap);

    // remove the trailing newline if any
    if (char_count > 0 && char_count < (int)sizeof(buffer) && buffer[char_count - 1] == '\n') {
        buffer[char_count - 1] = '\0';
    }

    // log
    GG_LOG_FINEST("%s", buffer);

    va_end(ap);
}
