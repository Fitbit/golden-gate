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
 * @date 2018-03-26
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Utils Utilities
//! Data sink that prints metadata about what it receives
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
/**
 * Printing sink object.
 */
typedef struct GG_PrintDataSink GG_PrintDataSink;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
/**
 * When this flag is set in the options, the buffer metadata will be printed
 */
#define GG_PRINT_DATA_SINK_OPTION_PRINT_METADATA  1

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a Printing data sink.
 *
 * @param options Or'ed combination of option flags.
 * @param max_payload_print Maximum number of bytes from each packet to print.
 * @param sink Address of the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the object was created, or a negative error code.
 */
GG_Result GG_PrintDataSink_Create(uint32_t            options,
                                  size_t              max_payload_print,
                                  GG_PrintDataSink**  sink);

/**
 * Destroy a Printing data sink.
 *
 * @param self The object on which this method is invoked.
 */
void GG_PrintDataSink_Destroy(GG_PrintDataSink* self);

/**
 * Get the GG_DataSink interface for the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_DataSink interface for the object.
 */
GG_DataSink* GG_PrintDataSink_AsDataSink(GG_PrintDataSink* self);

//!@}

#if defined(__cplusplus)
}
#endif
