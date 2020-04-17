/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2017-11-20
 *
 * @details
 *
 * Mynewt implementation of the log config interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_system.h"
#include "xp/common/gg_strings.h"

#include "nvm.h"

/*----------------------------------------------------------------------
|   defines
+---------------------------------------------------------------------*/
#define LOG_CONFIG_DEFAULT_STRING   "plist:.handlers=ConsoleHandler,AnnotationHandler;.level=ALL"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
GG_Result GG_System_GetLogConfig(GG_String* config)
{
    char buf[LOG_CONFIG_MAX_LEN + 1];
    nvm_error_t rc;

    rc = nvm_get_log_config(buf, sizeof(buf));
    if (rc == NVM_OK) {
        GG_String_Assign(config, buf);
    } else {
        GG_String_Assign(config, LOG_CONFIG_DEFAULT_STRING);
    }

    return GG_SUCCESS;
}
