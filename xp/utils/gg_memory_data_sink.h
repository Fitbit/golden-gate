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
 * @date 2017-12-08
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Utils Utilities
//! Utility classes and functions, typically used in examples and tests.
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_MemoryDataSink GG_MemoryDataSink;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
GG_Result GG_MemoryDataSink_Create(GG_MemoryDataSink** sink);
void GG_MemoryDataSink_Destroy(GG_MemoryDataSink* self);

GG_DataSink* GG_MemoryDataSink_AsDataSink(GG_MemoryDataSink* self);
GG_Buffer* GG_MemoryDataSink_GetBuffer(GG_MemoryDataSink* self);

/**
 * Reset the state of the object.
 * This clears the internal buffer.
 */
void GG_MemoryDataSink_Reset(GG_MemoryDataSink* self);

//!@}

#if defined(__cplusplus)
}
#endif
