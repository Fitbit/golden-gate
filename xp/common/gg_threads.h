/**
 *
 * @file
 * @brief Abstraction layer for thread-related functions
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-27
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_utils.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Threads
//! Classes and functions related the multi-threading
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Thread identifier.
 */
typedef uintptr_t GG_ThreadId;

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
/**
 * Use this macro in an object field declaration to enable thread binding
 */
#define GG_THREAD_GUARD_ENABLE_BINDING GG_ThreadId _bound_thread_id;

/**
 * Bind an object to the current thread ID
 * > NOTE: the object must support thread binding, which is done by using
 * the GG_THREAD_GUARD_ENABLE_BINDING macro in the object declaration.
 */
#define GG_THREAD_GUARD_BIND(_object) do {                 \
    (_object)->_bound_thread_id = GG_GetCurrentThreadId(); \
} while (0)

/**
 * Returns whether or not the current thread is the object's bound thread
 */
#define GG_THREAD_GUARD_IS_CURRENT_THREAD_BOUND(_object) \
((_object)->_bound_thread_id == GG_GetCurrentThreadId())

/**
 * Returns whether or not an object is bound to a thread
 */
#define GG_THREAD_GUARD_IS_OBJECT_BOUND(_object) \
((_object)->_bound_thread_id != 0)

#if defined(GG_CONFIG_ENABLE_THREAD_GUARDS)

#if defined(GG_CONFIG_THREAD_GUARDS_ASSERT)
#define GG_THREAD_GUARD_CHECK(_x) GG_ASSERT(_x)
#else
#define GG_THREAD_GUARD_CHECK(_x) (_x)
#endif

/**
 * Check that the current thread ID matches the thread ID to which an object is bound
 */
#define GG_THREAD_GUARD_CHECK_BINDING(_object) do {                                                        \
    GG_THREAD_GUARD_CHECK(GG_ThreadGuard_CheckCurrentThreadIsExpected((_object)->_bound_thread_id, NULL)); \
} while (0)

/**
 * This macro can be used to check that the current thread is the one
 * expected when invoking a certain function or method
 */
#define GG_THREAD_GUARD_CHECK_MAIN_LOOP() do {\
    GG_THREAD_GUARD_CHECK(GG_ThreadGuard_CheckCurrentThreadIsMainLoop(NULL)); \
} while (0)

#else

#define GG_THREAD_GUARD_CHECK_BINDING(_object)
#define GG_THREAD_GUARD_CHECK_MAIN_LOOP()

#endif

/**
 * Sets the ID of the thread used to compare against when calling
 * GG_ThreadGuard_CheckCurrentThreadIsMainLoop().
 * By default, the target thread ID is set to 0, which is used as
 * a special value that matches all threads.
 *
 * @param thread_id ID of the thread ID to check against, or 0.
 */
void GG_ThreadGuard_SetMainLoopThreadId(GG_ThreadId thread_id);

/**
 * Check that the current thread matches the target set previously.
 * This function is a convenience function to allow logging and
 * setting breakpoints when a mismatch is detected.
 *
 * @param caller_name Name of the caller (for logging), or NULL.
 *
 * @return `true` if the current thread matches the target thread ID or
 * if the target thread ID is 0; returns `false` otherwise.
 */
bool GG_ThreadGuard_CheckCurrentThreadIsMainLoop(const char* caller_name);

/**
 * Check that the current thread matches an expected thread.
 * This function is a convenience function to allow logging and
 * setting breakpoints when a mismatch is detected.
 *
 * @param thread_id The expected thread ID.
 * @param caller_name Name of the caller (for logging), or NULL.
 *
 * @return `true` if the current thread matches the expected thread ID
 * or `false` otherwise.
 */
bool GG_ThreadGuard_CheckCurrentThreadIsExpected(GG_ThreadId thread_id,
                                                 const char* caller_name);

//---------------------------------------------------------------------
//! @class GG_Mutex
//
//! Mutex object that can be used to prevent concurrent access to data structures
//! from multiple threads.
//---------------------------------------------------------------------
typedef struct GG_Mutex GG_Mutex;

/**
 * Create a mutex object
 *
 * @param mutex Pointer to a variable in which the mutex will be returned.
 * @return GG_SUCCESS if the mutex could be created, or an error code.
 */
GG_Result
GG_Mutex_Create(GG_Mutex** mutex);

/**
 * Mutex locking/creation
 *
 * This is useful for creating/locking mutexes that are used as
 * singletons for example.
 * if *mutex is NULL, the mutex will be created in a thread-safe manner
 * (it is ok to have several threads try to create the same mutex at
 * the same time), and will then be locked. Otherwise, it will just be locked.
 *
 * @param mutex Pointer to a variable in which the mutex will be returned.
 * @return #GG_SUCCESS if the mutex could be created, or an error code.
 *
 * @relates GG_Mutex
 */
GG_Result
GG_Mutex_LockAutoCreate(GG_Mutex** mutex);

/**
 * Lock a mutex.
 *
 * @param self The object on which this method is called.
 * @return #GG_SUCCESS if the mutex could be locked, or an error code.
 *
 * @relates GG_Mutex
 */
GG_Result
GG_Mutex_Lock(GG_Mutex* self);

/**
 * Unlock a mutex.
 *
 * @param self The object on which this method is called.
 * @return #GG_SUCCESS if the mutex could be locked, or an error code.
 *
 * @relates GG_Mutex
 */
GG_Result
GG_Mutex_Unlock(GG_Mutex* self);

/**
 * Destroy a mutex.
 *
 * @param self The object on which this method is called.
 *
 * @relates GG_Mutex
 */
void
GG_Mutex_Destroy(GG_Mutex* self);

/**
 * Get the identifier of the current thread.
 *
 * @return The identifier of the current thread.
 */
GG_ThreadId
GG_GetCurrentThreadId(void);

//---------------------------------------------------------------------
//! @class GG_Semaphore
//
//! Semaphore object that can be used to control access to a shared resource
//! from multiple threads.
//---------------------------------------------------------------------
typedef struct GG_Semaphore GG_Semaphore;

/**
 * Create a semaphore object
 *
 * @param initial_value Initial value of the semaphore.
 * @param semaphore Pointer to a variable in which the semaphore will be returned.
 * @return GG_SUCCESS if the semaphore could be created, or an error code.
 */
GG_Result
GG_Semaphore_Create(unsigned int initial_value, GG_Semaphore** semaphore);

/**
 * Acquire a semaphore.
 * This method blocks while the semaphore value is 0, then decrements the value.
 *
 * @param self The object on which this method is called.
 *
 * @relates GG_Semaphore
 */
void
GG_Semaphore_Acquire(GG_Semaphore* self);

/**
 * Release a semaphore.
 * This method increments the semaphore value.
 *
 * @param self The object on which this method is called.
 *
 * @relates GG_Semaphore
 */
void
GG_Semaphore_Release(GG_Semaphore* self);

/**
 * Destroy a semaphore.
 *
 * @param self The object on which this method is called.
 *
 * @relates GG_Semaphore
 */
void
GG_Semaphore_Destroy(GG_Semaphore* self);

//! @}

#if defined(__cplusplus)
}
#endif
