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
 * @date 2017-09-25
 *
 * @details
 *
 * Event loop.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_loop.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_LinkedListNode  list_node;
    GG_Buffer*         data;
    GG_BufferMetadata* metadata;
} GG_LoopDataSinkProxyQueueItem;

struct GG_LoopDataSinkProxy {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_LoopMessage);

    GG_Mutex*                      mutex;
    GG_Loop*                       loop;
    GG_DataSink*                   sink;
    GG_DataSinkListener*           listener;
    unsigned int                   queue_length;
    GG_LinkedList                  queue;
    GG_LinkedList                  queue_item_pool;
    GG_LoopDataSinkProxyQueueItem* queue_items;
    bool                           queue_has_waiter;
};

struct GG_LoopDataSinkListenerProxy {
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_LoopMessage);

    GG_Loop*             loop;
    GG_DataSinkListener* listener;
};

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.loop")

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_LoopEventHandler_OnEvent(GG_LoopEventHandler* self, GG_Loop* loop) {
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnEvent(self, loop);
}

//----------------------------------------------------------------------
void
GG_LoopMessage_Handle(GG_LoopMessage* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->Handle(self);
}

//----------------------------------------------------------------------
void
GG_LoopMessage_Release(GG_LoopMessage* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->Release(self);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_LoopDataSinkProxy_TryToPutData(GG_LoopDataSinkProxy* self)
{
    // lock
    GG_Mutex_Lock(self->mutex);

    // forward all queued data to the sink
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->queue) {
        GG_LoopDataSinkProxyQueueItem* item = GG_LINKED_LIST_ITEM(node, GG_LoopDataSinkProxyQueueItem, list_node);
        GG_Result result = GG_DataSink_PutData(self->sink, item->data, item->metadata);

        // stop now if we couldn't put the data
        if (GG_FAILED(result)) {
            break;
        }

        // the data was accepted by the sink, so we can recycle the item
        GG_LINKED_LIST_NODE_REMOVE(node);
        GG_LINKED_LIST_APPEND(&self->queue_item_pool, node);

        // and release the buffer + metadata
        GG_Buffer_Release(item->data);
        if (item->metadata) {
            GG_FreeMemory(item->metadata);
        }
    }
    bool should_notify = self->queue_has_waiter && !GG_LINKED_LIST_IS_EMPTY(&self->queue_item_pool);

    // unlock
    GG_Mutex_Unlock(self->mutex);

    if (should_notify && self->listener) {
        GG_DataSinkListener_OnCanPut(self->listener);
    }
}

//----------------------------------------------------------------------
static void
GG_LoopDataSinkProxy_AsLoopMessage_Handle(GG_LoopMessage* _self)
{
    GG_LoopDataSinkProxy* self = GG_SELF(GG_LoopDataSinkProxy, GG_LoopMessage);

    GG_LoopDataSinkProxy_TryToPutData(self);
}

//----------------------------------------------------------------------
static void
GG_LoopDataSinkProxy_AsLoopMessage_Release(GG_LoopMessage* self)
{
    GG_COMPILER_UNUSED(self);
    // nothing to do here since the message interface is implemented
    // by the proxy object itself, and that object is long-lived.
}

//----------------------------------------------------------------------
static GG_Result
GG_LoopDataSinkProxy_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_LoopDataSinkProxy* self   = GG_SELF(GG_LoopDataSinkProxy, GG_DataSink);
    GG_Result             result = GG_SUCCESS;

    // lock
    GG_Mutex_Lock(self->mutex);

    // try to get an item from the pool
    if (GG_LINKED_LIST_IS_EMPTY(&self->queue_item_pool)) {
        // no item available from the pool, we'll need to try again later
        result = GG_ERROR_WOULD_BLOCK;
        self->queue_has_waiter = true;
        goto end;
    }

    // clone the buffer (because buffer reference counting isn't thread safe, we can't pass a
    // reference to the buffer to a different thread)
    GG_DynamicBuffer* cloned_data      = NULL;
    GG_BufferMetadata* cloned_metadata = NULL;
    size_t data_size = GG_Buffer_GetDataSize(data);
    result = GG_DynamicBuffer_Create(data_size, &cloned_data);
    if (GG_FAILED(result)) {
        goto end;
    }
    GG_DynamicBuffer_SetData(cloned_data, GG_Buffer_GetData(data), data_size);
    result = GG_BufferMetadata_Clone(metadata, &cloned_metadata);
    if (GG_FAILED(result)) {
        GG_DynamicBuffer_Release(cloned_data);
        goto end;
    }

    // if we got to here, we're not waiting anymore
    self->queue_has_waiter = false;

    // get an item from the pool and queue it
    bool queue_was_empty = GG_LINKED_LIST_IS_EMPTY(&self->queue);
    GG_LinkedListNode* node = GG_LINKED_LIST_HEAD(&self->queue_item_pool);
    GG_LINKED_LIST_NODE_REMOVE(node);
    GG_LINKED_LIST_APPEND(&self->queue, node);

    // setup the item
    GG_LoopDataSinkProxyQueueItem* item = GG_LINKED_LIST_ITEM(node, GG_LoopDataSinkProxyQueueItem, list_node);
    item->data     = GG_DynamicBuffer_AsBuffer(cloned_data);
    item->metadata = cloned_metadata;

    // if the list was previously empty, try to post to the message queue without blocking in order
    // to process the list in the message handler
    if (queue_was_empty) {
        result = GG_Loop_PostMessage(self->loop, GG_CAST(self, GG_LoopMessage), 0);

        // check the result
        if (result == GG_ERROR_TIMEOUT) {
            // the data has been queued, but we couldn't post a message to the loop
            // to tell it to process the queue
            // TODO: register with the loop to be called back when we can re-post
            GG_LOG_WARNING("unable to post message to loop");
            result = GG_SUCCESS; // for now, don't return an error, because the data is already in the queue
        }
    }

end:
    // unlock
    GG_Mutex_Unlock(self->mutex);

    return result;
}

//----------------------------------------------------------------------
static GG_Result
GG_LoopDataSinkProxy_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_LoopDataSinkProxy* self = GG_SELF(GG_LoopDataSinkProxy, GG_DataSink);

    // keep a pointer to the listener
    self->listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// This method is called by the proxy'ed sink.
// Try to submit any pending buffers to the sink.
//----------------------------------------------------------------------
static void
GG_LoopDataSinkProxy_OnCanPut(GG_DataSinkListener* _self)
{
    GG_LoopDataSinkProxy* self = GG_SELF(GG_LoopDataSinkProxy, GG_DataSinkListener);

    // put what we can
    GG_LoopDataSinkProxy_TryToPutData(self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LoopDataSinkProxy, GG_DataSink) {
    GG_LoopDataSinkProxy_PutData,
    GG_LoopDataSinkProxy_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_LoopDataSinkProxy, GG_DataSinkListener) {
    GG_LoopDataSinkProxy_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_LoopDataSinkProxy, GG_LoopMessage) {
    GG_LoopDataSinkProxy_AsLoopMessage_Handle,
    GG_LoopDataSinkProxy_AsLoopMessage_Release
};

//----------------------------------------------------------------------
GG_Result
GG_Loop_CreateDataSinkProxy(GG_Loop*               loop,
                            unsigned int           queue_size,
                            GG_DataSink*           sink,
                            GG_LoopDataSinkProxy** sink_proxy)
{
    GG_ASSERT(loop);
    GG_ASSERT(sink);
    GG_ASSERT(sink_proxy);

    // check parameters
    if (queue_size == 0 || queue_size > GG_LOOP_DATA_SINK_PROXY_MAX_QUEUE_LENGTH) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // allocate a new object
    *sink_proxy = GG_AllocateZeroMemory(sizeof(GG_LoopDataSinkProxy));
    if (*sink_proxy == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // allocate queue items
    (*sink_proxy)->queue_items =
        (GG_LoopDataSinkProxyQueueItem*)GG_AllocateMemory(queue_size * sizeof(GG_LoopDataSinkProxyQueueItem));
    if ((*sink_proxy)->queue_items == NULL) {
        GG_FreeMemory(*sink_proxy);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the object
    (*sink_proxy)->loop = loop;
    (*sink_proxy)->sink = sink;
    GG_Mutex_Create(&(*sink_proxy)->mutex);

    // init the pool
    GG_LINKED_LIST_INIT(&(*sink_proxy)->queue);
    GG_LINKED_LIST_INIT(&(*sink_proxy)->queue_item_pool);
    for (unsigned int i = 0; i < queue_size; i++) {
        GG_LINKED_LIST_APPEND(&(*sink_proxy)->queue_item_pool, &(*sink_proxy)->queue_items[i].list_node);
    }

    // setup interfaces
    GG_SET_INTERFACE(*sink_proxy, GG_LoopDataSinkProxy, GG_DataSink);
    GG_SET_INTERFACE(*sink_proxy, GG_LoopDataSinkProxy, GG_DataSinkListener);
    GG_SET_INTERFACE(*sink_proxy, GG_LoopDataSinkProxy, GG_LoopMessage);

    // register as a listener with the sink
    GG_DataSink_SetListener(sink, GG_CAST(*sink_proxy, GG_DataSinkListener));

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_LoopDataSinkProxy_Destroy(GG_LoopDataSinkProxy* self)
{
    if (self == NULL) return;

    // de-register as a listener from the sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    // release resources
    if (self->mutex) {
        GG_Mutex_Destroy(self->mutex);
    }
    if (self->queue_items) {
        GG_FreeMemory(self->queue_items);
    }

    // free the object memory
    GG_ClearAndFreeObject(self, 3);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_LoopDataSinkProxy_AsDataSink(GG_LoopDataSinkProxy* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
static void
GG_LoopDataSinkListenerProxy_AsLoopMessage_Handle(GG_LoopMessage* _self)
{
    GG_LoopDataSinkListenerProxy* self = GG_SELF(GG_LoopDataSinkListenerProxy, GG_LoopMessage);

    // call the original listener
    if (self->listener) {
        GG_DataSinkListener_OnCanPut(self->listener);
    }
}

//----------------------------------------------------------------------
static void
GG_LoopDataSinkListenerProxy_AsLoopMessage_Release(GG_LoopMessage* self)
{
    GG_COMPILER_UNUSED(self);
    // nothing to do here since the message interface is implemented
    // by the proxy object itself, and that object is long-lived.
}

//----------------------------------------------------------------------
static void
GG_LoopDataSinkListenerProxy_OnCanPut(GG_DataSinkListener* _self)
{
    GG_LoopDataSinkListenerProxy* self = GG_SELF(GG_LoopDataSinkListenerProxy, GG_DataSinkListener);

    // try to post to the message queue without blocking
    // TODO: we need to register with the loop to get a chance to re-post later when there's space
    GG_Loop_PostMessage(self->loop, GG_CAST(self, GG_LoopMessage), 0);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LoopDataSinkListenerProxy, GG_DataSinkListener) {
    GG_LoopDataSinkListenerProxy_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_LoopDataSinkListenerProxy, GG_LoopMessage) {
    GG_LoopDataSinkListenerProxy_AsLoopMessage_Handle,
    GG_LoopDataSinkListenerProxy_AsLoopMessage_Release
};

//----------------------------------------------------------------------
GG_Result
GG_Loop_CreateDataSinkListenerProxy(GG_Loop*                       self,
                                    GG_DataSinkListener*           listener,
                                    GG_LoopDataSinkListenerProxy** listener_proxy)
{
    GG_ASSERT(self);
    GG_ASSERT(listener);
    GG_ASSERT(listener_proxy);

    // allocate a new object
    *listener_proxy = GG_AllocateZeroMemory(sizeof(GG_LoopDataSinkListenerProxy));
    if (*listener_proxy == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // initialize the object
    (*listener_proxy)->loop     = self;
    (*listener_proxy)->listener = listener;

    // setup interfaces
    GG_SET_INTERFACE(*listener_proxy, GG_LoopDataSinkListenerProxy, GG_DataSinkListener);
    GG_SET_INTERFACE(*listener_proxy, GG_LoopDataSinkListenerProxy, GG_LoopMessage);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_LoopDataSinkListenerProxy_Destroy(GG_LoopDataSinkListenerProxy* self)
{
    if (self == NULL) return;

    GG_ClearAndFreeObject(self, 2);
}

//----------------------------------------------------------------------
GG_DataSinkListener*
GG_LoopDataSinkListenerProxy_AsDataSinkListener(GG_LoopDataSinkListenerProxy* self)
{
    return GG_CAST(self, GG_DataSinkListener);
}
