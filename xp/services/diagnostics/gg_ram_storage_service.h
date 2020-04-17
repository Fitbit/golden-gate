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
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Services Services
//! Diagnostics RAM Storage service
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/remote/gg_remote.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_RAMStorageService GG_RAMStorageService;

/**
 * Function prototype for adding a dummy record to RAM storage.
 */
typedef void (*GG_Diagnostics_AddDummyRecord)(void);

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_RAM_STORAGE_SERVICE_GENERATE_RECORDS_METHOD  "diagnostics/generate_records"
#define GG_RAM_STORAGE_SERVICE_GET_RECORDS_METHOD       "diagnostics/get_records"
#define GG_RAM_STORAGE_SERVICE_DELETE_RECORDS_METHOD    "diagnostics/delete_records"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Create a RAM Storage service object.
 *
 * @note: Size of buffer allocated for diagnostics/get_records Remote API response
 * can be limited with max_get_size. If max_get_size is 0, no limit is imposed.
 *
 * @param storage [in] Pointer to GG_RAMStorage object handled by this service
 * @param dummy_func [in] Function for generating dummy records.
 * @param max_get_size [in] Maximum number of bytes returned by diagnostics/get_records.
 * @param service [out] Address of the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_RAMStorageService_Create(GG_RAMStorage *storage,
                                      GG_Diagnostics_AddDummyRecord dummy_func,
                                      uint16_t max_get_size,
                                      GG_RAMStorageService** service);

/**
 * Destroy a RAM Storage service object.
 *
 * @param self The object on which the method is invoked.
 */
void GG_RAMStorageService_Destroy(GG_RAMStorageService* self);

/**
 * Get a reference to the RAM Storage service GG_RemoteSmoHandler object
 *
 * @param self The object on which the method is invoked.
 *
 * @return Pointer to GG_RemoteSmoHandler object
 */
GG_RemoteSmoHandler* GG_RAMStorageService_AsRemoteSmoHandler(GG_RAMStorageService* self);

/**
 * Register the RAM Storage service with a remote API shell.
 * This function may only be called from the same thread as the one in which the shell is
 * running.
 */
GG_Result GG_RAMStorageService_Register(GG_RAMStorageService* self, GG_RemoteShell* shell);

//!@}

#if defined(__cplusplus)
}
#endif
