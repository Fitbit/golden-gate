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
 * @date 2018-04-26
 *
 * @details
 *
 * No-op Autorelease support
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_results.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_AutoreleaseWrap(GG_Result (*function)(void* arg), void* arg)
{
    return (*function)(arg);
}
