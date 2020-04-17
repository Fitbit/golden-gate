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
 * Standard C implementation of the console interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>

#include "xp/common/gg_system.h"
#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   GG_System_ConsoleOutput
+---------------------------------------------------------------------*/
void
GG_System_ConsoleOutput(const char* string)
{
    printf("%s", string);
}
