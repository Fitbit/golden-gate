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
 * Golden Gate Diagnostics RAM Record Storage implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_lists.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_ring_buffer.h"
#include "xp/common/gg_threads.h"

#include "gg_diagnostics_ram_storage.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Structure for storing data related to a record array handle
 *
 * A new GG_Record_Handle is created (with an unique handle number) each
 * time GG_Diagnostics_RAMStorage_GetHandle is called. The handle has a count
 * equal to the number of records present in storage, which signifies number
 * of records tracked by the handle. The storage maintains a linked list of all
 * the handles valid at any time.
 *
 * Each time a record is removed from storage, the count and offset of every handle
 * is decremented (which signifies that the handle tracks one less record). As
 * such, the count for each handle represents how many records from the storage
 * are tracked by that handle at any time, and the offset the number of records
 * that need to be skipped when retriving records from storage. If the count reaches
 * 0, the handle becomes invalid and is removed from the linked list.
 *
 * When removing records with GG_Diagnostics_RAMStorage_DeleteRecords using a handle,
 * only the current count of records for that handle will be removed. This way,
 * a remove operation will only remove those records that were in storage at
 * the time the handle was created.
 */
typedef struct {
    GG_LinkedListNode list_node;    // GG Linked List node
    uint16_t handle;                // Handle number
    uint16_t count;                 // Number of records from storage tracked by handle
    uint16_t offset;                // Number of records already retrieved for handle
} GG_Record_Handle;

/**
 * Internal stucture for handling RAM Storage
 */
struct GG_RAMStorage {
    GG_RingBuffer buffer;               // Ring buffer for storing records
    uint8_t *data;                      // Byte array used by ring buffer
    uint16_t byte_size;                 // Size in bytes of data array
    uint16_t record_count;              // Number of records present in storage

    GG_Mutex *mutex;                    // Mutex for making operations atomic

    GG_LinkedList handles;              // Linked list for with all valid handles
    uint16_t next_handle;               // Next handle number to be created
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_Diagnostics_RAMStorage_Handles_RecordsRemoved(GG_RAMStorage *self,
                                                 uint16_t      removed_count)
{
    GG_Record_Handle *handle;

    if (removed_count == 0) {
        return;
    }

    // Decrease count in each handle node and keep only handles that track
    // at least one record still present in storage
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->handles) {
        handle = GG_LINKED_LIST_ITEM(node, GG_Record_Handle, list_node);

        if (handle->count <= removed_count) {
            GG_LINKED_LIST_NODE_REMOVE(node);
            GG_FreeMemory(handle);
        } else {
            handle->count -= removed_count;
            if (handle->offset > removed_count) {
                handle->offset -= removed_count;
            } else {
                handle->offset = 0;
            }
        }
    }
}

//----------------------------------------------------------------------
static GG_Record_Handle*
GG_Diagnostics_RAMStorage_GetRecordHandle(GG_RAMStorage *self, uint16_t handle)
{
    // Search for handle in linked list
    GG_LINKED_LIST_FOREACH(node, &self->handles) {
        GG_Record_Handle *h = GG_LINKED_LIST_ITEM(node, GG_Record_Handle, list_node);

        if (h->handle == handle) {
            return h;
        }
    }

    return NULL;
}

//----------------------------------------------------------------------
static GG_Record_Handle*
GG_Diagnostics_RAMStorage_CreateNewHandle(GG_RAMStorage *self)
{
    GG_Record_Handle *handle_p;

    handle_p = GG_AllocateZeroMemory(sizeof(GG_Record_Handle));
    if (handle_p == NULL) {
        return NULL;
    }

    handle_p->handle = self->next_handle;
    handle_p->count = self->record_count;
    handle_p->offset = 0;

    // Add new handle to beginning of list to optimize search in GG_Diagnostics_RAMStorage_DeleteRecords
    GG_LINKED_LIST_PREPEND(&self->handles, &handle_p->list_node);

    if (self->next_handle == GG_DIAGNOSTICS_RECORD_HANDLE_MAX) {
        self->next_handle = GG_DIAGNOSTICS_RECORD_HANDLE_MIN;
    } else {
        self->next_handle++;
    }

    return handle_p;
}

//----------------------------------------------------------------------
GG_Result
GG_Diagnostics_RAMStorage_Create(uint16_t size, GG_RAMStorage **storage)
{
    GG_RAMStorage *self;
    GG_Result rc;

    self = GG_AllocateZeroMemory(sizeof(GG_RAMStorage));
    if (self == NULL) {
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    self->data = GG_AllocateZeroMemory(size);
    if (self->data == NULL) {
        GG_FreeMemory(self);
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    rc = GG_Mutex_Create(&self->mutex);
    if (rc != GG_SUCCESS) {
        GG_FreeMemory(self->data);
        GG_FreeMemory(self);
        return rc;
    }

    GG_RingBuffer_Init(&self->buffer, self->data, size);
    GG_LINKED_LIST_INIT(&self->handles);

    self->next_handle = GG_DIAGNOSTICS_RECORD_HANDLE_MIN;
    self->record_count = 0;
    self->byte_size = size;

    *storage = self;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Diagnostics_RAMStorage_Destroy(GG_RAMStorage *self)
{
    if (self == NULL) return;

    GG_Diagnostics_RAMStorage_Handles_RecordsRemoved(self, self->record_count);
    GG_FreeMemory(self->data);
    GG_Mutex_Destroy(self->mutex);
    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
GG_Result
GG_Diagnostics_RAMStorage_AddRecord(GG_RAMStorage *self,
                                    void          *payload,
                                    uint16_t      payload_len)
{
    size_t free_bytes;
    uint16_t removed_count;
    uint16_t record_size;
    uint16_t len;

    if (payload_len == 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    GG_Mutex_Lock(self->mutex);

    // Check if there is space for the record in the buffer
    record_size = sizeof(payload_len) + payload_len;

    if (record_size > self->byte_size) {
        GG_Mutex_Unlock(self->mutex);
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    free_bytes = GG_RingBuffer_GetSpace(&self->buffer);
    removed_count = 0;

    // Remove old records to make room for new record
    while (free_bytes < record_size) {
        GG_RingBuffer_Read(&self->buffer, (uint8_t *)&len, sizeof(len));
        GG_RingBuffer_MoveOut(&self->buffer, len);
        removed_count++;

        free_bytes = GG_RingBuffer_GetSpace(&self->buffer);
    }

    self->record_count -= removed_count;
    GG_Diagnostics_RAMStorage_Handles_RecordsRemoved(self, removed_count);

    // Add record to buffer
    GG_RingBuffer_Write(&self->buffer, (uint8_t *)&payload_len, sizeof(payload_len));
    GG_RingBuffer_Write(&self->buffer, (uint8_t *)payload, payload_len);
    self->record_count++;

    GG_Mutex_Unlock(self->mutex);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
uint16_t
GG_Diagnostics_RAMStorage_GetRecordCount(GG_RAMStorage *self)
{
    uint16_t count;

    GG_Mutex_Lock(self->mutex);
    count = self->record_count;
    GG_Mutex_Unlock(self->mutex);

    return count;
}

//----------------------------------------------------------------------
GG_Result
GG_Diagnostics_RAMStorage_GetRecords(GG_RAMStorage *self,
                                     uint16_t      *handle,
                                     uint16_t      *size,
                                     uint8_t       *records)
{
    GG_Record_Handle *handle_p = NULL;
    uint16_t payload_len;
    uint16_t byte_offset = 0;
    uint16_t max_return_recs;
    uint16_t ret_len = 0;
    uint16_t count;
    uint16_t i;

    GG_Mutex_Lock(self->mutex);

    if (*handle == GG_DIAGNOSTICS_RECORD_HANDLE_GENERATE) {
        handle_p = GG_Diagnostics_RAMStorage_CreateNewHandle(self);
        if (handle_p == NULL) {
            GG_Mutex_Unlock(self->mutex);
            return GG_ERROR_OUT_OF_MEMORY;
        }

        *handle = handle_p->handle;
    } else if (*handle != GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE) {
        handle_p = GG_Diagnostics_RAMStorage_GetRecordHandle(self, *handle);
        if (handle_p == NULL) {
            GG_Mutex_Unlock(self->mutex);
            *size = 0;
            return GG_SUCCESS;
        }

        // All records already retrieved
        if (handle_p->count == handle_p->offset) {
            GG_Mutex_Unlock(self->mutex);
            *size = 0;
            return GG_SUCCESS;
        }

        // Skip already retrieved records
        for (i = 0; i < handle_p->offset; i++) {
            GG_RingBuffer_Peek(&self->buffer, (uint8_t *)&payload_len,
                               byte_offset, sizeof(payload_len));
            byte_offset += sizeof(payload_len) + payload_len;
        }
    }

    // Compute maximum records to return
    if (*handle == GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE) {
        max_return_recs = self->record_count;
    } else {
        max_return_recs = handle_p->count - handle_p->offset;
    }

    for (i = 0; i < max_return_recs; i++) {
        GG_RingBuffer_Peek(&self->buffer, (uint8_t *)&payload_len,
                           byte_offset, sizeof(payload_len));

        // Check for room to store new record
        if (ret_len + sizeof(payload_len) + payload_len > *size) {
            break;
        }

        GG_RingBuffer_Peek(&self->buffer, records + ret_len,
                           byte_offset, sizeof(payload_len) + payload_len);
        ret_len += sizeof(payload_len) + payload_len;
        byte_offset += sizeof(payload_len) + payload_len;
    }

    count = i;

    if (*handle == GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE) {
        // Remove retrieved records from storage
        for (i = 0; i < count; i++) {
            GG_RingBuffer_Peek(&self->buffer, (uint8_t *)&payload_len, 0, sizeof(payload_len));
            GG_RingBuffer_MoveOut(&self->buffer, sizeof(payload_len) + payload_len);
        }

        self->record_count -= count;
        GG_Diagnostics_RAMStorage_Handles_RecordsRemoved(self, count);
    } else {
        handle_p->offset += count;
    }

    GG_Mutex_Unlock(self->mutex);

    *size = ret_len;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Diagnostics_RAMStorage_DeleteRecords(GG_RAMStorage *self, uint16_t handle)
{
    GG_Record_Handle *handle_p;
    uint16_t payload_len;

    if (handle == GG_DIAGNOSTICS_RECORD_HANDLE_GENERATE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    GG_Mutex_Lock(self->mutex);

    if (handle == GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE) {
        // Remove all records from storage
        GG_RingBuffer_Reset(&self->buffer);
        GG_Diagnostics_RAMStorage_Handles_RecordsRemoved(self, self->record_count);
        self->record_count = 0;

        GG_Mutex_Unlock(self->mutex);
        return GG_SUCCESS;
    }

    handle_p = GG_Diagnostics_RAMStorage_GetRecordHandle(self, handle);
    if (handle_p == NULL) {
        // Invalid handle or handle was removed from handle list when all records tracked by it
        // were removed from storage
        GG_Mutex_Unlock(self->mutex);
        return GG_SUCCESS;
    }

    // Remove records
    for (uint16_t i = 0; i < handle_p->count; i++) {
        GG_RingBuffer_Peek(&self->buffer, (uint8_t *)&payload_len, 0, sizeof(payload_len));
        GG_RingBuffer_MoveOut(&self->buffer, sizeof(payload_len) + payload_len);
    }

    self->record_count -= handle_p->count;
    GG_Diagnostics_RAMStorage_Handles_RecordsRemoved(self, handle_p->count);

    GG_Mutex_Unlock(self->mutex);

    return GG_SUCCESS;
}
