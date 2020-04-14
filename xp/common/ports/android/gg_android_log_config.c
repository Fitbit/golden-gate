/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-06
 *
 * @details
 *
 * Android implementation of the log config interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <sys/system_properties.h>
#include <android/log.h>
#include <dlfcn.h>

#include "xp/common/gg_system.h"
#include "xp/common/gg_strings.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_system.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_ANDROID_LOG_CONFIG_PROP_NAME         "debug.gg.log.config"
#define GG_ANDROID_LOG_TAG                      "GoldenGate"
#define GG_ANDROID_DEFAULT_LOG_CONFIG           "plist:.level=INFO"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
// manually load the  __system_property_get function from the lib, because
// the linker won't find it
//----------------------------------------------------------------------
static int
android_property_get(const char* name, char* value) {
    static int (*system_property_get)(const char*, char*) = NULL;
    if (system_property_get == NULL) {
        void* libc_handle = dlopen("libc.so", 0);
        if (!libc_handle) {
             __android_log_print(ANDROID_LOG_DEBUG,
                                 GG_ANDROID_LOG_TAG,
                                 "cannot dlopen libc.so: %s",
                                 dlerror());
             return 0;
        }
        system_property_get = (int (*)(const char*, char*))(dlsym(libc_handle, "__system_property_get"));
        if (!system_property_get) {
             __android_log_print(ANDROID_LOG_DEBUG,
                                 GG_ANDROID_LOG_TAG,
                                 "cannot resolve __system_property_get(): %s",
                                 dlerror());
             return 0;
        }
    }
    return (*system_property_get)(name, value);
}

//----------------------------------------------------------------------
GG_Result
GG_System_GetLogConfig(GG_String* config)
{
    char gg_log_config[PROP_VALUE_MAX];
    gg_log_config[0] = 0;
    int prop_len = android_property_get(GG_ANDROID_LOG_CONFIG_PROP_NAME, gg_log_config);
    if (prop_len > 0) {
        GG_String_AssignN(config, gg_log_config, prop_len);
        __android_log_print(ANDROID_LOG_DEBUG,
                            GG_ANDROID_LOG_TAG,
                            "gg_log_config: %s\n",
                            GG_String_GetChars(config));
    } else {
        GG_String_Assign(config, GG_ANDROID_DEFAULT_LOG_CONFIG);
        __android_log_write(ANDROID_LOG_DEBUG,
                            GG_ANDROID_LOG_TAG,
                            "gg_log_config: DEFAULT\n");
    }
    return GG_SUCCESS;
}
