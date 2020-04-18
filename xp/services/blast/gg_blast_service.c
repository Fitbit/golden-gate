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
 * @date 2018-04-10
 *
 * @details
 * Blast service implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_threads.h"
#include "xp/smo/gg_smo_allocator.h"
#include "gg_blast_service.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Blast service main object
 */
struct GG_BlastService {
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    GG_Loop*              loop;
    GG_DataSource*        stack_source;
    GG_DataSink*          stack_sink;
    GG_PerfDataSink*      perf_sink;
    GG_BlasterDataSource* blaster_source;

    GG_THREAD_GUARD_ENABLE_BINDING
};

typedef struct {
    GG_BlastService* self;
} GG_BlastServiceInitInvokeArgs;

typedef struct {
    GG_BlastService* self;
} GG_BlastServiceDeinitInvokeArgs;

typedef struct {
    GG_BlastService* self;
    GG_DataSource*   source;
    GG_DataSink*     sink;
} GG_BlastServiceAttachInvokeArgs;

typedef struct {
    GG_BlastService* self;
    size_t           packet_count;
    size_t           packet_interval;
    size_t           packet_size;
} GG_BlastServiceStartBlasterInvokeArgs;

typedef struct {
    GG_BlastService* self;
} GG_BlastServiceStopBlasterInvokeArgs;

typedef struct {
    GG_BlastService*      self;
    GG_PerfDataSinkStats* stats;
} GG_BlastServiceGetStatsInvokeArgs;

typedef struct {
    GG_BlastService* self;
} GG_BlastServiceResetStatsInvokeArgs;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
// Shared RPC request handler for all the methods
//----------------------------------------------------------------------
static GG_Result
GG_BlastService_HandleRequest(GG_RemoteSmoHandler* _self,
                              const char*          request_method,
                              Fb_Smo*              request_params,
                              GG_JsonRpcErrorCode* rpc_error_code,
                              Fb_Smo**             rpc_result)
{
    GG_BlastService* self = GG_SELF(GG_BlastService, GG_RemoteSmoHandler);
    GG_COMPILER_UNUSED(rpc_error_code);

    if (!strcmp(request_method, GG_BLAST_SERVICE_START_METHOD)) {
        // extrat the parameter objects
        Fb_Smo* packet_count_p    = Fb_Smo_GetChildByName(request_params, "packet_count");
        Fb_Smo* packet_interval_p = Fb_Smo_GetChildByName(request_params, "packet_interval");
        Fb_Smo* packet_size_p     = Fb_Smo_GetChildByName(request_params, "packet_size");

        // check that we have the required parameters
        if (packet_size_p == NULL) {
            return GG_ERROR_INVALID_PARAMETERS;
        }

        // convert the parameter objects
        size_t packet_count    = packet_count_p ? (size_t)Fb_Smo_GetValueAsInteger(packet_count_p) : 0;
        size_t packet_interval = packet_interval_p ? (size_t)Fb_Smo_GetValueAsInteger(packet_interval_p) : 0;
        size_t packet_size     = (size_t)Fb_Smo_GetValueAsInteger(packet_size_p);

        // check the parameter values
        if (packet_size < GG_BLASTER_IP_COUNTER_PACKET_MIN_SIZE) {
            return GG_ERROR_INVALID_PARAMETERS;
        }

        // start the blaster
        return GG_BlastService_StartBlaster(self, packet_size, packet_count, packet_interval);
    } else if (!strcmp(request_method, GG_BLAST_SERVICE_STOP_METHOD)) {
        return GG_BlastService_StopBlaster(self);
    } else if (!strcmp(request_method, GG_BLAST_SERVICE_GET_STATS_METHOD)) {
        GG_PerfDataSinkStats stats;
        GG_Result result;
        int rc;

        result = GG_BlastService_GetStats(self, &stats);
        if (GG_FAILED(result)) {
            return GG_FAILURE;
        }

        Fb_Smo* result_smo = Fb_Smo_Create(&GG_SmoHeapAllocator,
                                           "{bytes_received=ipackets_received=igap_count=i}",
                                           (int)stats.bytes_received,
                                           (int)stats.packets_received,
                                           (int)stats.gap_count);
        if (result_smo == NULL) {
            return GG_FAILURE;
        }

        // Create and add throughput smo outside of Fb_Smo_Create,
        // as va_arg(double) doesn't work as expected on some ARM platforms
        Fb_Smo* throughput_smo = Fb_Smo_CreateFloat(&GG_SmoHeapAllocator,
                                                    (double)stats.throughput);
        if (throughput_smo == NULL) {
            Fb_Smo_Destroy(result_smo);
            return GG_FAILURE;
        }

        rc = Fb_Smo_AddChild(result_smo, "throughput", (unsigned int)strlen("throughput"), throughput_smo);
        if (rc != FB_SMO_SUCCESS) {
            Fb_Smo_Destroy(result_smo);
            Fb_Smo_Destroy(throughput_smo);
            return GG_FAILURE;
        }

        *rpc_result = result_smo;
        return GG_SUCCESS;
    } else if (!strcmp(request_method, GG_BLAST_SERVICE_RESET_STATS_METHOD)) {
        return GG_BlastService_ResetStats(self);
    }

    return GG_FAILURE;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_BlastService, GG_RemoteSmoHandler) {
    .HandleRequest = GG_BlastService_HandleRequest
};

//----------------------------------------------------------------------
// Invoked by GG_BlastService_Create (called in the GG loop thread context)
//----------------------------------------------------------------------
static int
GG_BlastService_Init(void* _args)
{
    GG_BlastServiceInitInvokeArgs* args = _args;
    GG_BlastService*               self = args->self;

    // create a perf data sink
    GG_Result result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER,
                                              GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_CONSOLE,
                                              GG_MILLISECONDS_PER_SECOND,
                                              &self->perf_sink);
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Invoked by GG_BlastService_Destroy (called in the GG loop thread context)
//----------------------------------------------------------------------
static int
GG_BlastService_Deinit(void* _args)
{
    GG_BlastServiceDeinitInvokeArgs* args = _args;
    GG_BlastService*                 self = args->self;

    // disconnect and cleanup
    if (self->stack_source) {
        GG_DataSource_SetDataSink(self->stack_source, NULL);
    }
    GG_PerfDataSink_Destroy(self->perf_sink);
    GG_BlasterDataSource_Destroy(self->blaster_source);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Create a GG_BlastService object (called in any thread context,
// typically the GG_RemoteShell thread context)
//----------------------------------------------------------------------
GG_Result
GG_BlastService_Create(GG_Loop*          loop,
                       GG_BlastService** service)
{
    // allocate a new object
    GG_BlastService* self = (GG_BlastService*)GG_AllocateZeroMemory(sizeof(GG_BlastService));
    if (self == NULL) {
        *service = NULL;
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object
    self->loop = loop;
    GG_BlastServiceInitInvokeArgs invoke_args = {
        .self = self
    };
    int invoke_result = 0;
    GG_Result result = GG_Loop_InvokeSync(loop, GG_BlastService_Init, &invoke_args, &invoke_result);
    if (GG_FAILED(result) || invoke_result != 0) {
        GG_FreeMemory(self);
        return GG_FAILED(result) ? result : invoke_result;
    }

    // setup interfaces
    GG_SET_INTERFACE(self, GG_BlastService, GG_RemoteSmoHandler);

    // bind the object to the thread that createed it
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *service = self;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Destroy a GG_BlastService object (called in any thread context,
// typically the GG_RemoteShell thread context)
//----------------------------------------------------------------------
void
GG_BlastService_Destroy(GG_BlastService* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    // deinit
    GG_BlastServiceDeinitInvokeArgs invoke_args = {
        .self = self
    };
    int invoke_result = 0;
    (void)GG_Loop_InvokeSync(self->loop, GG_BlastService_Deinit, &invoke_args, &invoke_result);

    // release the memory
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_RemoteSmoHandler*
GG_BlastService_AsRemoteSmoHandler(GG_BlastService* self)
{
    return GG_CAST(self, GG_RemoteSmoHandler);
}

//----------------------------------------------------------------------
GG_Result
GG_BlastService_Register(GG_BlastService* self, GG_RemoteShell* shell)
{
    GG_Result result;

    // register RPC methods
    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_BLAST_SERVICE_START_METHOD,
                                               GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_BLAST_SERVICE_STOP_METHOD,
                                               GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_BLAST_SERVICE_GET_STATS_METHOD,
                                               GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_BLAST_SERVICE_RESET_STATS_METHOD,
                                               GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_BlastService_Unregister(GG_BlastService* self, GG_RemoteShell* shell)
{
    GG_Result result;

    // unregister RPC methods
    result = GG_RemoteShell_UnregisterSmoHandler(shell,
                                                 GG_BLAST_SERVICE_START_METHOD,
                                                 GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_RemoteShell_UnregisterSmoHandler(shell,
                                                 GG_BLAST_SERVICE_STOP_METHOD,
                                                 GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_RemoteShell_UnregisterSmoHandler(shell,
                                                 GG_BLAST_SERVICE_GET_STATS_METHOD,
                                                 GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }
    result = GG_RemoteShell_UnregisterSmoHandler(shell,
                                                 GG_BLAST_SERVICE_RESET_STATS_METHOD,
                                                 GG_BlastService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Invoked by GG_BlastService_Attach (called in the GG loop thread context)
//----------------------------------------------------------------------
static int
GG_BlastService_Attach_(void* _args)
{
    GG_BlastServiceAttachInvokeArgs* args = _args;
    GG_BlastService*                 self = args->self;

    // detach from the current source and sink
    if (self->blaster_source) {
        GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(self->blaster_source), NULL);
    }
    if (self->stack_source) {
        GG_DataSource_SetDataSink(self->stack_source, NULL);
    }

    // destroy blaster source if the sink is NULL & there's nothing attached
    if (!args->sink && self->blaster_source) {
        GG_BlasterDataSource_Destroy(self->blaster_source);
        self->blaster_source = NULL;
    }

    // update the source and sink references
    self->stack_sink   = args->sink;
    self->stack_source = args->source;

    // re-connect the source and sink if needed
    if (self->blaster_source) {
        GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(self->blaster_source), self->stack_sink);
    }
    if (self->stack_source && self->perf_sink) {
        GG_DataSource_SetDataSink(self->stack_source, GG_PerfDataSink_AsDataSink(self->perf_sink));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_BlastService_Attach(GG_BlastService* self, GG_DataSource* source, GG_DataSink* sink)
{
    GG_BlastServiceAttachInvokeArgs invoke_args = {
        .self   = self,
        .source = source,
        .sink   = sink
    };
    int invoke_result = 0;
    GG_Result result = GG_Loop_InvokeSync(self->loop, GG_BlastService_Attach_, &invoke_args, &invoke_result);
    if (GG_FAILED(result)) {
        return result;
    }
    return invoke_result;
}

//----------------------------------------------------------------------
// Invoked by GG_BlastService_GetStats (called in the GG loop thread context)
//----------------------------------------------------------------------
static int
GG_BlastService_GetStats_(void* _args)
{
    GG_BlastServiceGetStatsInvokeArgs* args = _args;
    GG_BlastService*                   self = args->self;

    *args->stats = *GG_PerfDataSink_GetStats(self->perf_sink);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_BlastService_GetStats(GG_BlastService* self, GG_PerfDataSinkStats* stats)
{
    GG_BlastServiceGetStatsInvokeArgs invoke_args = {
        .self  = self,
        .stats = stats
    };
    int invoke_result = 0;
    GG_Result result = GG_Loop_InvokeSync(self->loop, GG_BlastService_GetStats_, &invoke_args, &invoke_result);
    if (GG_FAILED(result)) {
        return result;
    }
    return invoke_result;
}

//----------------------------------------------------------------------
// Invoked by GG_BlastService_ResetStats (called in the GG loop thread context)
//----------------------------------------------------------------------
static int
GG_BlastService_ResetStats_(void* _args)
{
    GG_BlastServiceResetStatsInvokeArgs* args = _args;
    GG_BlastService*                     self = args->self;

    GG_PerfDataSink_ResetStats(self->perf_sink);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_BlastService_ResetStats(GG_BlastService* self)
{
    GG_BlastServiceResetStatsInvokeArgs invoke_args = {
        .self = self
    };
    int invoke_result = 0;
    GG_Result result = GG_Loop_InvokeSync(self->loop, GG_BlastService_ResetStats_, &invoke_args, &invoke_result);
    if (GG_FAILED(result)) {
        return result;
    }
    return invoke_result;
}

//----------------------------------------------------------------------
// Invoked by GG_BlastService_StartBlaster (called in the GG loop thread context)
//----------------------------------------------------------------------
static int
GG_BlastService_StartBlaster_(void* _args)
{
    GG_BlastServiceStartBlasterInvokeArgs* args = _args;
    GG_BlastService*                       self = args->self;

    // create and start a blaster only if we're attached to a valid sink
    if (self->stack_sink == NULL) {
        return GG_FAILURE;
    }

    // destroy the current source
    if (self->blaster_source) {
        GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(self->blaster_source), NULL);
        GG_BlasterDataSource_Destroy(self->blaster_source);
        self->blaster_source = NULL;
    }

    // create a new source
    GG_Result result = GG_BlasterDataSource_Create(args->packet_size,
                                                   GG_BLASTER_IP_COUNTER_PACKET_FORMAT,
                                                   args->packet_count,
                                                   GG_Loop_GetTimerScheduler(self->loop),
                                                   (unsigned int)args->packet_interval,
                                                   &self->blaster_source);
    if (GG_FAILED(result)) {
        return result;
    }

    // connect the source
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(self->blaster_source), self->stack_sink);

    // start the source
    GG_BlasterDataSource_Start(self->blaster_source);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_BlastService_StartBlaster(GG_BlastService* self,
                             size_t           packet_size,
                             size_t           packet_count,
                             size_t           packet_interval)
{
    GG_BlastServiceStartBlasterInvokeArgs invoke_args = {
        .self            = self,
        .packet_size     = packet_size,
        .packet_count    = packet_count,
        .packet_interval = packet_interval
    };
    int invoke_result = 0;
    GG_Result result = GG_Loop_InvokeSync(self->loop, GG_BlastService_StartBlaster_, &invoke_args, &invoke_result);
    if (GG_FAILED(result)) {
        return result;
    }
    return invoke_result;
}

//----------------------------------------------------------------------
// Invoked by GG_BlastService_StopBlaster (called in the GG loop thread context)
//----------------------------------------------------------------------
static int
GG_BlastService_StopBlaster_(void* _args)
{
    GG_BlastServiceStopBlasterInvokeArgs* args = _args;
    GG_BlastService*                      self = args->self;

    if (self->blaster_source) {
        GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(self->blaster_source), NULL);
        GG_BlasterDataSource_Destroy(self->blaster_source);
        self->blaster_source = NULL;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_BlastService_StopBlaster(GG_BlastService* self)
{
    GG_BlastServiceResetStatsInvokeArgs invoke_args = {
        .self = self
    };
    int invoke_result = 0;
    GG_Result result = GG_Loop_InvokeSync(self->loop, GG_BlastService_StopBlaster_, &invoke_args, &invoke_result);
    if (GG_FAILED(result)) {
        return result;
    }
    return invoke_result;
}
