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
 * @date 2019-04-02
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Utils Utilities
//! Monitor data source
//! @{

/*----------------------------------------------------------------------
 |   includes
 +---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_events.h"
#include "xp/common/gg_timer.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_ActivityDataMonitor GG_ActivityDataMonitor;

/**
 * Change type for activity monitor event
 */
typedef enum {
    GG_ACTIVITY_MONITOR_CHANGE_TYPE_BOTTOM_TO_TOP_ACTIVE,
    GG_ACTIVITY_MONITOR_CHANGE_TYPE_BOTTOM_TO_TOP_INACTIVE,
    GG_ACTIVITY_MONITOR_CHANGE_TYPE_TOP_TO_BOTTOM_ACTIVE,
    GG_ACTIVITY_MONITOR_CHANGE_TYPE_TOP_TO_BOTTOM_INACTIVE
} GG_ActivityMonitorChangeType;

/**
 * Event emitted when a activity monitor element detected an activity changed.
 *
 * (cast a GG_Event to that type when the event's type ID is GG_EVENT_TYPE_ACTIVITY_MONITOR_CHANGE)
 */
typedef struct {
    GG_Event        base;
    uint32_t        direction;      ///< Direction of the activity being monitored.
    bool            active;         ///< Whether activity was detected or not.
    GG_Timestamp    detected_time;  ///< Timestamp when change was detected.
} GG_ActivityMonitorChangeEvent;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
/**
 * Event type emitted by an Activity Monitor when a change in activity is
 * detected.
 *
 * The event struct is a GG_ActivityMonitorChangeEvent.
 * The event source is the GG_ActivityMonitor object that emits the event.
 */
#define GG_EVENT_TYPE_ACTIVITY_MONITOR_CHANGE GG_4CC('a', 'm', 'o', 'c')

#define GG_ACTIVITY_MONITOR_DIRECTION_BOTTOM_TO_TOP 0 ///< Activity Monitor for bottom to top direction
#define GG_ACTIVITY_MONITOR_DIRECTION_TOP_TO_BOTTOM 1 ///< Activity Monitor for top to bottom direction

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a activity data monitor.
 *
 * @param scheduler Timer Scheduler to use for scheduling internal timer.
 * @param direction The direction of the activity being monitored to use in a
 *        GG_ActivityMonitorChangeEvent (GG_ACTIVITY_MONITOR_DIRECTION_BOTTOM_TO_TOP or
 *        GG_ACTIVITY_MONITOR_DIRECTION_TOP_TO_BOTTOM).
 * @param inactivity_timeout The amount of time in ms to wait before considering the source has
 *        become inactive.
 * @param source Pointer to the variable that will receive the object.
 *
 * @return GG_SUCCESS if the object was created, or a negative error code.
 */
GG_Result GG_ActivityDataMonitor_Create(GG_TimerScheduler*       scheduler,
                                        uint32_t                 direction,
                                        uint32_t                 inactivity_timeout,
                                        GG_ActivityDataMonitor** source);

/**
 * Destroy a activity data monitor.
 *
 * @param self The object on which this method is invoked.
 */
void GG_ActivityDataMonitor_Destroy(GG_ActivityDataMonitor* self);

/**
 * Get the GG_DataSource interface for the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_DataSource interface for the object.
 */
GG_DataSource* GG_ActivityDataMonitor_AsDataSource(GG_ActivityDataMonitor* self);

/**
 * Get the GG_DataSink interface for the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_DataSink interface for the object.
 */
GG_DataSink* GG_ActivityDataMonitor_AsDataSink(GG_ActivityDataMonitor* self);


/**
 * Return the GG_EventEmitter interface of the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_EventEmitter interface of the object.
 */
GG_EventEmitter* GG_ActivityDataMonitor_AsEventEmitter(GG_ActivityDataMonitor* self);

/**
 * Return the GG_Inspectable interface of the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_Inspectable interface of the object.
 */
GG_Inspectable* GG_ActivityDataMonitor_AsInspectable(GG_ActivityDataMonitor* self);

//!@}

#if defined(__cplusplus)
}
#endif
