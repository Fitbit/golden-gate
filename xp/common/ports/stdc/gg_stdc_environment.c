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
 * Standard C implementation of the system environment interface.
 */
 
/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "xp/common/gg_system.h"
#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   GG_GetEnvironment
+---------------------------------------------------------------------*/
GG_Result 
GG_System_GetEnvironment(const char* name, GG_String* value)
{
    char* env;

    /* default value */
    GG_String_SetLength(value, 0);

#if defined(GG_CONFIG_HAVE_GETENV)
    env = getenv(name);
    if (env) {
        GG_String_Assign(value, env);
        return GG_SUCCESS;
    } else {
        return GG_ERROR_NO_SUCH_ITEM;
    }
#elif defined(GG_CONFIG_HAVE_DUPENV_S)
    if (dupenv_s(&env, NULL, name) != 0) {
        return GG_FAILURE;
    } else if (env != NULL) {
        GG_String_Assign(value, env);
        free(env);
        return GG_SUCCESS;
    } else {
        return GG_ERROR_NO_SUCH_ITEM;
    }
#else
#error "no getenv or getenv_s available on this platform"
#endif
}
