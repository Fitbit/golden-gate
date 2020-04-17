
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
 * @date 2018-12-26
 *
 * @details
 *
 * Core Bluetooth transport interface
 */

//----------------------------------------------------------------------
// includes
//----------------------------------------------------------------------
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"

//----------------------------------------------------------------------
// functions
//----------------------------------------------------------------------
typedef struct GG_StackToolMacosTunnel GG_StackToolMacosTunnel;

//----------------------------------------------------------------------
// functions
//----------------------------------------------------------------------
GG_Result GG_StackToolMacosTunnel_Create(GG_Loop*                  loop,
                                         bool                      trace,
                                         GG_StackToolMacosTunnel** tunnel);
void GG_StackToolMacosTunnel_Destroy(GG_StackToolMacosTunnel* self);
GG_DataSource* GG_StackToolMacosTunnel_AsDataSource(GG_StackToolMacosTunnel* self);
GG_DataSink* GG_StackToolMacosTunnel_AsDataSink(GG_StackToolMacosTunnel* self);
