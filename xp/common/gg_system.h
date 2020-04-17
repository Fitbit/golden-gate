/**
 * @file
 * @brief Abstraction layer for system/platform specific functions
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_strings.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup System
//! Classes and functions related to the system.
//! @{

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Get a system environment variable.
 *
 * @param name Name of the environment variable to read.
 * @param value GG_String object that will receive the value of the variable if found.
 * @return #GG_SUCCESS if the environment variable was found, or an error code.
 */
GG_Result GG_System_GetEnvironment(const char* name, GG_String* value);

/**
 * Get the defaut system logging configuration.
 *
 * @param config GG_String object that will receive the value if one exists.
 * @return #GG_SUCCESS if a default logging configuration exists, or an error code.
 */
GG_Result GG_System_GetLogConfig(GG_String* config);

/**
 * Get the current system timestamp.
 *
 * NOTE: timestamp values are relative to an unspecified time origin, thus should
 * not be used to represent absolute time values.
 *
 * @return The current system timestamp.
 */
GG_Timestamp GG_System_GetCurrentTimestamp(void);

/**
 * Get the current system wall clock time.
 *
 * @return GG_Timestamp in nanoseconds, as an offset from the Epoch.
 */
GG_Timestamp GG_System_GetWallClockTime(void);

/**
 * Output a string to the default system console.
 *
 * @param string String to output.
 */
void GG_System_ConsoleOutput(const char* string);

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
/**
 * Get the current system wall clock time in milliseconds as an offset from the Epoch.
 */
#define GG_System_GetWallClockTimeMs() \
    (GG_System_GetWallClockTime() / GG_NANOSECONDS_PER_MILLISECOND)

//! @}

#if defined(__cplusplus)
}
#endif
