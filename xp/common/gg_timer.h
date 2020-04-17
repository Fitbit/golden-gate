/**
 * @file
 * @brief Timers
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2017-09-11
 *
 * @details
 * An API for scheduling future work at a certain time in the future
 */
#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Timers
//! Timers
//! @{

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_TIMER_NEVER ((uint32_t)-1) ///< Value used to represent an infinite time in the future.

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Timer object that can be scheduled to fire at a specific time.
 * When a timer fires, a GG_TimerListener listener is called.
 */
typedef struct GG_Timer GG_Timer;

//----------------------------------------------------------------------
//! Interface implemented by objects that can be called when a timer fires.
//!
//! @interface GG_TimerListener
//! @see GG_Timer_Schedule
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_TimerListener) {
    //! Called whenever a scheduled timer has fired
    //!
    //! @param self The Listener being invoked
    //! @param timer The timer object being acted on
    //! @param time_elapsed The actual number of milliseconds that have elapsed since the timer was
    //! scheduled
    void (*OnTimerFired)(GG_TimerListener* self, GG_Timer* timer, uint32_t time_elapsed);
};

//! @var GG_TimerListener::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_TimerListenerInterface
//! Virtual function table for the GG_TimerListener interface

//! @relates GG_TimerListener
//! @copydoc GG_TimerListenerInterface::OnTimerFired
void GG_TimerListener_OnTimerFired(GG_TimerListener* self, GG_Timer* timer, uint32_t actual_ms_elapsed);

//! A timer scheduler is an object that creates and manages timers
typedef struct GG_TimerScheduler GG_TimerScheduler;

//! Deletes a created timer
//!
//! @param[in] self Frees up any memory associated with the timer. The timer
//!            can no longer be used after this call is complete
void GG_Timer_Destroy(GG_Timer* self);

//! Schedules a timer to execute in the future.
//!
//! @note Calling this while the timer is already scheduled will simply reschedule the timer
//! @param self The timer to schedule
//! @param listener An implementation of the interface that will be used to notify consumer about
//!                 timer events
//! @param ms_from_now The time in ms for the timer to fire
//! @return #GG_SUCCESS if the timer was scheduled or an error code on failure
GG_Result GG_Timer_Schedule(GG_Timer* self, GG_TimerListener* listener, uint32_t ms_from_now);

//! Checks to see if the requested timer is scheduled
//!
//! @param self The timer to run the check on
//! @returns true if the timer is scheduled
bool GG_Timer_IsScheduled(GG_Timer* self);

//! Unschedules a timer. If the timer is not actually scheduled, this is a no-op
//!
//! @param self The timer to unschedule
void GG_Timer_Unschedule(GG_Timer* self);

//! Get the remaining time before the timeout fires
//!
//! @param self The timer to get the remaining time
//! @return the remaining time in milliseconds or 0 if the timer is not active or has already expired
uint32_t GG_Timer_GetRemainingTime(GG_Timer *self);

//! Create a new timer scheduler
GG_Result GG_TimerScheduler_Create(GG_TimerScheduler** scheduler);

//! Destroy a timer scheduler.
//! (all timers that have been created by this scheduler and have not yet been
//! destroyed will be destroyed)
void GG_TimerScheduler_Destroy(GG_TimerScheduler* self);

//! Create a timer.
GG_Result GG_TimerScheduler_CreateTimer(GG_TimerScheduler* self, GG_Timer** timer);

//! Set the current time of the scheduler.
//! This causes all timers that are scheduled for a time prior or equal to that time to fire.
//! NOTE: upon creation, the timer scheduler's time is 0.
GG_Result GG_TimerScheduler_SetTime(GG_TimerScheduler* self, uint32_t now);

//! Get the current time of the scheduler.
uint32_t GG_TimerScheduler_GetTime(GG_TimerScheduler* self);

//! Returns the number of milliseconds after which the next scheduled timer is set
//! to fire, or #GG_TIMER_NEVER if no timer is scheduled.
uint32_t GG_TimerScheduler_GetNextScheduledTime(GG_TimerScheduler* self);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
