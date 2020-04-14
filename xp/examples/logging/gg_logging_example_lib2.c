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
GG_SET_LOCAL_LOGGER("foo.bar")

//---------------------------------------------------------------------------------------
void
example_func2(void)
{
    GG_LOG_FATAL("Hello from example_func2");
}
