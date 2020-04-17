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
 * No-op implementation of the system environment interface.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_results.h"
#include "xp/common/gg_strings.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result GG_System_GetEnvironment(const char* name, GG_String* value)
{
    GG_COMPILER_UNUSED(name);
    GG_COMPILER_UNUSED(value);

    return GG_ERROR_NOT_SUPPORTED;
}
