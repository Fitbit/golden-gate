/**
 * @file
 * @brief Convenience include file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-20
 *
 * @details
 *
 * This convenience include file pulls in all the sub-module header
 * files from xp/common, so that one can simply #include "xp/common/gg_common.h"
 * instead of the individual header files one by one.
 */

#pragma once

//! @addtogroup Convenience Convenience
//! Convenience Include Files
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_crc32.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_ring_buffer.h"
#include "xp/common/gg_strings.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_events.h"
#include "xp/common/gg_version.h"
#include "xp/common/gg_inspect.h"
#include "xp/common/gg_bitstream.h"

//! @}
