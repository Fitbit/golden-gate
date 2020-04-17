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
 * @date 2018-10-12
 *
 * @details
 *
 * FreeRTOS implementation of the system time interface
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "xp/common/gg_system.h"
#include "xp/common/gg_utils.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Timestamp
GG_System_GetCurrentTimestamp(void)
{
    uint64_t tick_count = xTaskGetTickCount();
    return (GG_Timestamp)(((uint64_t)portTICK_PERIOD_MS * tick_count) * GG_NANOSECONDS_PER_MILLISECOND);
}
