/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-08
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Utils Utilities
//! Asynchronous data pipe
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_timer.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
//! @class GG_AsyncPipe
//
//! Class that implements a simple store-and-forward pipe, in one direction.
//!
//! It may be used, for example, when connecting two elements that want to
//! communicate asynchronously (i.e without GG_DataSink_PutData() directly calling
//! the GG_DataSink::PutData method on the sink, but instead having that method invoked
//! in the next timer tick.)
//!
//! This object exposes a GG_DataSource and a GG_DataSink interface.
//! Calling GG_DataSink_PutData causes the data to be received by the source's
//! sink on the next timer tick
//!
//! ```
//!                  +
//!                  |
//!                  |
//!              +---v----+
//!              |  sink  |
//!              +--------+
//!              |        |    +-----------------+
//!              | Buffer |<-->| Timer Scheduler |
//!              |        |    +-----------------+
//!              +--------+
//!              | source |
//!              +---+----+
//!                  |
//!                  |
//!                  v
//! ```

//----------------------------------------------------------------------
typedef struct GG_AsyncPipe GG_AsyncPipe;

/**
 * Create the object.
 *
 * @param timer_scheduler Timer scheduler used to create a tick timer for pumping
 * the data between the endpoints.
 * @param [out] pipe Pointer to where the created object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_AsyncPipe_Create(GG_TimerScheduler* timer_scheduler, unsigned int max_items, GG_AsyncPipe** pipe);

/**
 * Destroy the object.
 *
 * @param self The object on which this method is invoked.
 */
void GG_AsyncPipe_Destroy(GG_AsyncPipe* self);

/**
 * Return the GG_DataSource interface of the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_DataSource interface of the object.
 */
GG_DataSource* GG_AsyncPipe_AsDataSource(GG_AsyncPipe* self);

/**
 * Return the GG_DataSink interface of the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_DataSink interface of the object.
 */
GG_DataSink* GG_AsyncPipe_AsDataSink(GG_AsyncPipe* self);

//!@}

#if defined(__cplusplus)
}
#endif
