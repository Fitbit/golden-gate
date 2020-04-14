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
 * Usage examples for the interfaces macros.
 */
 
/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>

#include "xp/common/gg_logging.h"
#include "gg_logging_example.h"

// set a local logger
GG_SET_LOCAL_LOGGER("foo.bar.y")

//---------------------------------------------------------------------------------------
void
example_func1(void)
{
    GG_LOG_FINEST("Hello %s", "Fitbit");
    GG_LOG_FINEST("Hello %s %s", "Golden", "Gate");
    GG_LOG_FINEST("one:%d, two:%d, three:%d", 1, 2, 3);
    GG_LOG_FINEST("%c%c%c%c", 'a', 'b', 'c', 'd');
    GG_LOG_FINEST("%02x %02x %02x %02x %02x", 1, 2, 3, 4, 5);
}
