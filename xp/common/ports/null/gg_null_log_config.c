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
 * No-op implementation of the log config interface
 */
 
/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_system.h"
#include "xp/common/gg_strings.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
GG_Result 
GG_System_GetLogConfig(GG_String* config)
{
    GG_String_SetLength(config, 0);
    return GG_ERROR_NOT_IMPLEMENTED;
}
