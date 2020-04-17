/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Sylvain Rebaud
 *
 * @date 2017-12-22
 *
 * @details
 *
 * Common library initialization.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_utils.h"
#include "xp/module/gg_module.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

static void GG_COMMON_Deinit(void* state) {
    GG_COMPILER_UNUSED(state);
    GG_LogManager_Terminate();
}

// we declare a local prototype for this because those init functions don't
// appear in any header file, they are referenced by macro construction
extern GG_Result GG_COMMON_Init(void);
GG_Result GG_COMMON_Init(void) {
    GG_LogManager_Initialize();

    static GG_SimpleCallback c1;
    GG_GenericCallbackHandler* h1 = GG_SimpleCallback_Init(&c1, GG_COMMON_Deinit, NULL);
    GG_Module_RegisterTerminationHandler(h1);

    return GG_SUCCESS;
}
