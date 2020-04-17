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
 * @date 2017-12-06
 *
 * @details
 *
 * Android implementation of the console interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <android/log.h>

#include "xp/common/gg_system.h"
#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   GG_System_ConsoleOutputt
+---------------------------------------------------------------------*/
void
GG_System_ConsoleOutput(const char* message)
{
    __android_log_write(ANDROID_LOG_DEBUG, "GoldenGate", message);
}
