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
 * @date 2019-11-24
 *
 * @details
 *
 * General purpose type utilities.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "gg_types.h"
#include "gg_logging.h"
#include "gg_port.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_CONFIG_MAX_INTERFACE_TRAP_SIZE 8

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.types")

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void
GG_GenericInterfaceTrapHandler(const void* self)
{
    GG_LOG_FATAL("interface trap called for object at %p", self);
    abort();
}

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
void (* const GG_GenericInterfaceTrapVTable[GG_CONFIG_MAX_INTERFACE_TRAP_SIZE])(const void*) = {
    GG_GenericInterfaceTrapHandler,
    GG_GenericInterfaceTrapHandler,
    GG_GenericInterfaceTrapHandler,
    GG_GenericInterfaceTrapHandler,
    GG_GenericInterfaceTrapHandler,
    GG_GenericInterfaceTrapHandler,
    GG_GenericInterfaceTrapHandler,
    GG_GenericInterfaceTrapHandler
};
