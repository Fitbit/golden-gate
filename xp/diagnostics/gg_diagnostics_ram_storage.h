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
 * @date 2018-06-22
 *
 * @details
 *
 * Golden Gate Diagnostics interface
 *
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"

//! @addtogroup Diagnostics Diagnostics
//! Device Diagnostics interface
//! @{

/*----------------------------------------------------------------------
|   defines
+---------------------------------------------------------------------*/

#define GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE        0x0000
#define GG_DIAGNOSTICS_RECORD_HANDLE_GENERATE      0x0001
#define GG_DIAGNOSTICS_RECORD_HANDLE_MIN           0x0002
#define GG_DIAGNOSTICS_RECORD_HANDLE_MAX           0xFFFF

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

typedef struct GG_RAMStorage GG_RAMStorage;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Create a Diagnostics RAM Storage object.
 *
 * @param size Size in bytes for the underlying storage buffer.
 * @param storage [out] Variable to return pointer to RAM Storage object that has been created.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_Diagnostics_RAMStorage_Create(uint16_t size, GG_RAMStorage **storage);

/**
 * Destroy a Diagnostics RAM Storage object.
 *
 * @param self The object on which the method is invoked.
 */
void GG_Diagnostics_RAMStorage_Destroy(GG_RAMStorage *self);

/**
 * Add record to RAM Storage.
 *
 * Add a record to the RAM Storage. If there is no space to add the new record,
 * the oldest records will be automatically removed to make space for it.
 *
 * @param self The object on which the method is invoked.
 * @param payload Pointer to record data to be added to storage.
 * @param payload_len Size in bytes of record data to be added to storage.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_Diagnostics_RAMStorage_AddRecord(GG_RAMStorage *self,
                                              void          *payload,
                                              uint16_t      payload_len);

/**
 * Get number of records in RAM Storage.
 *
 * @param self The object on which the method is invoked.
 * @return Number of records stored in RAM Storage.
 */
uint16_t GG_Diagnostics_RAMStorage_GetRecordCount(GG_RAMStorage *self);

/**
 * Get array of records from RAM Storage.
 *
 * Based on the value of size, not all records from storage may be returned
 * and this function needs to be called again with the same handle to return
 * the next records. When all records associated with a handle have been returned,
 * calling this function with that handle will return no records. The size in bytes
 * of the retrieved records is returned in the size parameter.
 *
 * If the handle value is GG_DIAGNOSTICS_RECORD_HANDLE_GENERATE, the value of a new handle
 * will be returned in the handle param. This handle will be associated with all the
 * records present in storage and can be used for the next calls for this function.
 *
 * If the handle value is GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE, the retrieved records
 * will be removed from storage.
 *
 * @note Records are retrieved from storage from oldest to newest.
 *
 * @param [in] self The object on which the method is invoked.
 * @param [in,out] handle Pointer to handle value.
 * @param [in,out] size Size of buffer for retrieving records
 * @param [out] records Pointer to record array containing requested records.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_Diagnostics_RAMStorage_GetRecords(GG_RAMStorage *self,
                                               uint16_t      *handle,
                                               uint16_t      *size,
                                               uint8_t       *records);

/**
 * Remove records from RAM Storage tracked by a record array handle.
 *
 * @note The handle can be obtained from a previous call to GG_Diagnostics_RAMStorage_GetRecords
 * @note If the handle value is GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE, all records will be removed from storage.
 *
 * @param self The object on which the method is invoked.
 * @param handle Handle which tracks records to be removed.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_Diagnostics_RAMStorage_DeleteRecords(GG_RAMStorage *self, uint16_t handle);

//! @}

#if defined(__cplusplus)
}
#endif
