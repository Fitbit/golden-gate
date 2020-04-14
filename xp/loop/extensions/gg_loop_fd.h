/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-25
 *
 * @details
 *
 * Loop extension for loops that can monitor file descriptors.
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_LoopEventHandlerItem base;
    int                     fd;
    uint32_t                event_mask;
    uint32_t                event_flags;
} GG_LoopFileDescriptorEventHandler;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
enum {
    GG_EVENT_FLAG_FD_CAN_READ  = 1,
    GG_EVENT_FLAG_FD_CAN_WRITE = 2,
    GG_EVENT_FLAG_FD_ERROR     = 4
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
#if defined(__cplusplus)
extern "C" {
#endif

GG_Result GG_Loop_AddFileDescriptorHandler(GG_Loop*                           self,
                                           GG_LoopFileDescriptorEventHandler* handler);

GG_Result GG_Loop_RemoveFileDescriptorHandler(GG_Loop*                           self,
                                              GG_LoopFileDescriptorEventHandler* handler);

#if defined(__cplusplus)
}
#endif
