/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-20
 *
 * @details
 *
 * Implementation of the global module functions
 */

 /*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "gg_module.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_MODULE_MAX_TERMINATION_HANDLERS 16

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    bool                       initialized;
    GG_GenericCallbackHandler* termination_handlers[GG_MODULE_MAX_TERMINATION_HANDLERS];
    size_t                     termination_handlers_count;
} GG_Module;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_Module _GoldenGateModule;

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
#define GG_MODULE_CALL_SUBMODULE_INIT(module)           \
    do {                                                \
        extern GG_Result GG_ ## module ## _Init(void);  \
        result = GG_ ## module ## _Init();              \
        if (GG_FAILED(result)) goto end;                \
    } while (0)

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_Module_Initialize(void)
{
    GG_Result result = GG_SUCCESS;

    if (_GoldenGateModule.initialized) {
        goto end; // use a goto here, so that we don't get a warning for unused labels
                  // in the case where there are no submodule init functions called below
    }
    _GoldenGateModule.initialized = true;

    // init all the submodules that need to be initialized
#if defined(GG_CONFIG_MODULE_INIT_BISON)
    GG_MODULE_CALL_SUBMODULE_INIT(BISON);
#endif

#if defined(GG_CONFIG_MODULE_INIT_COMMON)
    GG_MODULE_CALL_SUBMODULE_INIT(COMMON);
#endif

#if defined(GG_CONFIG_MODULE_INIT_LWIP)
    GG_MODULE_CALL_SUBMODULE_INIT(LWIP);
#endif

#if defined(GG_CONFIG_MODULE_INIT_MBEDTLS)
    GG_MODULE_CALL_SUBMODULE_INIT(MBEDTLS);
#endif

#if defined(GG_CONFIG_MODULE_INIT_WINSOCK)
    GG_MODULE_CALL_SUBMODULE_INIT(WINSOCK);
#endif

end:
    if (GG_FAILED(result)) {
        // one of the inits failed, cleanup before returning
        GG_Module_Terminate();
    }
    return result;
}

//----------------------------------------------------------------------
void
GG_Module_Terminate(void)
{
    // check that we're initialized
    if (!_GoldenGateModule.initialized) {
        return;
    }

    // call all the handlers in reverse order
    for (size_t i = _GoldenGateModule.termination_handlers_count; i; i--) {
        GG_GenericCallbackHandler_OnCallback(_GoldenGateModule.termination_handlers[i-1]);
    }

    // we're no longer initialized
    _GoldenGateModule.termination_handlers_count = 0;
    _GoldenGateModule.initialized = false;
}

//----------------------------------------------------------------------
GG_Result
GG_Module_RegisterTerminationHandler(GG_GenericCallbackHandler* handler)
{
    // check that we have space for one more
    if (_GoldenGateModule.termination_handlers_count == GG_MODULE_MAX_TERMINATION_HANDLERS) {
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // add this handler to the array
    _GoldenGateModule.termination_handlers[_GoldenGateModule.termination_handlers_count++] = handler;

    return GG_SUCCESS;
}
