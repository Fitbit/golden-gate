/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2018-07-04
 *
 * @details
 * Diagnostics RAM Storage service implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_memory.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_types.h"
#include "xp/diagnostics/gg_diagnostics_ram_storage.h"
#include "xp/smo/gg_smo_allocator.h"

#include "gg_ram_storage_service.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * RAM Storage service main object
 */
struct GG_RAMStorageService {
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    GG_RAMStorage *storage;
    GG_Diagnostics_AddDummyRecord dummy_func;
    uint16_t max_get_size;

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
// Shared RPC request handler for all the methods
//----------------------------------------------------------------------
static GG_Result
GG_RAMStorageService_HandleRequest(GG_RemoteSmoHandler* _self,
                                   const char*          request_method,
                                   Fb_Smo*              request_params,
                                   GG_JsonRpcErrorCode* rpc_error_code,
                                   Fb_Smo**             rpc_result)
{
    GG_RAMStorageService* self = GG_SELF(GG_RAMStorageService, GG_RemoteSmoHandler);

    GG_COMPILER_UNUSED(rpc_error_code);

    if (!strcmp(request_method, GG_RAM_STORAGE_SERVICE_GENERATE_RECORDS_METHOD)) {
        if (self->dummy_func == NULL) {
            return GG_ERROR_NOT_SUPPORTED;
        }

        Fb_Smo* count_p = Fb_Smo_GetChildByName(request_params, "count");
        if (count_p == NULL) {
            return GG_ERROR_INVALID_PARAMETERS;
        }

        uint16_t count = (uint16_t)Fb_Smo_GetValueAsInteger(count_p);

        // Add dummy records to storage
        for (int i = 0; i < count; i++) {
            self->dummy_func();
        }

        return GG_SUCCESS;
    } else if (!strcmp(request_method, GG_RAM_STORAGE_SERVICE_GET_RECORDS_METHOD)) {
        Fb_Smo* handle_p = Fb_Smo_GetChildByName(request_params, "handle");
        Fb_Smo* max_bytes_p = Fb_Smo_GetChildByName(request_params, "max_bytes");
        if (handle_p == NULL || max_bytes_p == NULL) {
            return GG_ERROR_INVALID_PARAMETERS;
        }

        int handle_val = (int)Fb_Smo_GetValueAsInteger(handle_p);
        uint16_t max_bytes = (uint16_t)Fb_Smo_GetValueAsInteger(max_bytes_p);

        if (self->max_get_size != 0 && max_bytes > self->max_get_size) {
            max_bytes = self->max_get_size;
        }

        uint8_t *buf = GG_AllocateZeroMemory(max_bytes);
        uint16_t handle = (uint16_t)handle_val;

        GG_Result rc = GG_Diagnostics_RAMStorage_GetRecords(self->storage, &handle, &max_bytes, buf);
        if (rc != GG_SUCCESS) {
            GG_FreeMemory(buf);
            return rc;
        }

        if (handle_val >= GG_DIAGNOSTICS_RECORD_HANDLE_MIN && handle_val <= GG_DIAGNOSTICS_RECORD_HANDLE_MAX) {
            *rpc_result  = Fb_Smo_Create(&GG_SmoHeapAllocator, "{handle=idata=b}",
                                         (int)handle, (uint8_t *)buf, (unsigned int)max_bytes);
        } else {
            *rpc_result  = Fb_Smo_Create(&GG_SmoHeapAllocator, "{data=b}",
                                         (uint8_t *)buf, (unsigned int)max_bytes);
        }

        GG_FreeMemory(buf);

        return GG_SUCCESS;
    } else if (!strcmp(request_method, GG_RAM_STORAGE_SERVICE_DELETE_RECORDS_METHOD)) {
        Fb_Smo* handle_p = Fb_Smo_GetChildByName(request_params, "handle");
        if (handle_p == NULL) {
            return GG_ERROR_INVALID_PARAMETERS;
        }

        int16_t handle = (int16_t)Fb_Smo_GetValueAsInteger(handle_p);

        return GG_Diagnostics_RAMStorage_DeleteRecords(self->storage, handle);
    }

    return GG_ERROR_NOT_SUPPORTED;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_RAMStorageService, GG_RemoteSmoHandler) {
    .HandleRequest = GG_RAMStorageService_HandleRequest
};

//----------------------------------------------------------------------
// Create a GG_RAMStorageService object (called in any thread context,
// typically the GG_RemoteShell thread context)
//----------------------------------------------------------------------
GG_Result
GG_RAMStorageService_Create(GG_RAMStorage *storage,
                            GG_Diagnostics_AddDummyRecord dummy_func,
                            uint16_t max_get_size,
                            GG_RAMStorageService** service)
{
    // allocate a new object
    GG_RAMStorageService* self = (GG_RAMStorageService*)GG_AllocateZeroMemory(sizeof(GG_RAMStorageService));
    if (self == NULL) {
        *service = NULL;
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup interfaces
    GG_SET_INTERFACE(self, GG_RAMStorageService, GG_RemoteSmoHandler);

    self->storage = storage;
    self->dummy_func = dummy_func;
    self->max_get_size = max_get_size;

    // bind the object to the thread that created it
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *service = self;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Destroy a GG_RAMStorageService object (called in any thread context,
// typically the GG_RemoteShell thread context)
//----------------------------------------------------------------------
void
GG_RAMStorageService_Destroy(GG_RAMStorageService* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    // release the memory
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_RemoteSmoHandler*
GG_RAMStorageService_AsRemoteSmoHandler(GG_RAMStorageService* self)
{
    return GG_CAST(self, GG_RemoteSmoHandler);
}

//----------------------------------------------------------------------
GG_Result
GG_RAMStorageService_Register(GG_RAMStorageService* self, GG_RemoteShell* shell)
{
    GG_Result result;

    // register RPC methods
    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_RAM_STORAGE_SERVICE_GENERATE_RECORDS_METHOD,
                                               GG_RAMStorageService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }

    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_RAM_STORAGE_SERVICE_GET_RECORDS_METHOD,
                                               GG_RAMStorageService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }

    result = GG_RemoteShell_RegisterSmoHandler(shell,
                                               GG_RAM_STORAGE_SERVICE_DELETE_RECORDS_METHOD,
                                               GG_RAMStorageService_AsRemoteSmoHandler(self));
    if (GG_FAILED(result)) {
        return result;
    }

    return GG_SUCCESS;
}
