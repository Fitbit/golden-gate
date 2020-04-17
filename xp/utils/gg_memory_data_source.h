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
//! Memory data source
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_buffer.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_MemoryDataSource GG_MemoryDataSource;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
GG_Result GG_MemoryDataSource_Create(GG_Buffer* data, size_t chunk_size, GG_MemoryDataSource** source);
void GG_MemoryDataSource_Destroy(GG_MemoryDataSource* self);

GG_DataSource* GG_MemoryDataSource_AsDataSource(GG_MemoryDataSource* self);
GG_Result GG_MemoryDataSource_Start(GG_MemoryDataSource* self);
size_t GG_MemoryDataSource_GetBytesLeft(GG_MemoryDataSource* self);
void GG_MemoryDataSource_Rewind(GG_MemoryDataSource* self);

//!@}

#if defined(__cplusplus)
}
#endif
