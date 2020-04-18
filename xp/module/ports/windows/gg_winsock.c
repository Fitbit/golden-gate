/**
 * @file
 * @brief Time functions for Windows
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-26
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <windows.h>

#include "xp/module/gg_module.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_WINSOCK_Cleanup(void* _ignored)
{
    WSACleanup();
}

//----------------------------------------------------------------------
GG_Result
GG_WINSOCK_Init(void)
{
    // initialize winsock, ask for version 2.2
    WORD    wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    int result = WSAStartup(wVersionRequested, &wsaData);
    if (result != 0) {
        return GG_FAILURE;
    }

    // register a cleanup function
    static GG_SimpleCallback cleanup_callback;
    GG_GenericCallbackHandler* cleanup_handler = GG_SimpleCallback_Init(&cleanup_callback, GG_WINSOCK_Cleanup, NULL);
    return GG_Module_RegisterTerminationHandler(cleanup_handler);
}
