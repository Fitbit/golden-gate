// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.NativeReferenceWithCallback
import com.fitbit.goldengate.bindings.coap.block.BlockwiseCoapResponseListener
import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest
import com.fitbit.goldengate.bindings.coap.handler.ResourceHandler
import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bindings.util.isNotNull
import io.reactivex.Completable
import io.reactivex.Scheduler
import io.reactivex.Single
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.concurrent.Executors
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Coap Client/Server API.
 *
 * Use [CoapEndpointProvider] for creating an instance
 */
class CoapEndpoint(
    customScheduler : Scheduler? = null
) : NativeReferenceWithCallback,
    StackService,
    Endpoint,
    DataSinkDataSource
{
    @get:Synchronized
    @set:Synchronized
    override var thisPointerWrapper: Long = 0
    override fun onFree() {
        thisPointerWrapper = 0
        Timber.d("free the native reference")
    }

    internal var dataSinkDataSource: DataSinkDataSource? = null
    private val resourceHandlerMap = mutableMapOf<String, Long>()
    val requestFilter: CoapGroupRequestFilter = CoapGroupRequestFilter()
    private val isInitialized = AtomicBoolean()
    private val scheduler : Scheduler = customScheduler ?: Schedulers.from(
        Executors.unconfigurableExecutorService(
            ThreadPoolExecutor(
                0,
                1,
                10,
                TimeUnit.SECONDS,
                LinkedBlockingQueue<Runnable>()
            )
        )
    )

    init {
        thisPointerWrapper = create()
        if (thisPointerWrapper.isNotNull()) {
            attachFilter(thisPointerWrapper, requestFilter.thisPointerWrapper)
            isInitialized.set(true)
        }
    }

    override fun responseFor(request: OutgoingRequest): Single<IncomingResponse> {
        return Single.create<IncomingResponse> { emitter ->
            val coapResponseListener: CoapResponseListener
            val responseForResult = if (!request.forceNonBlockwise) {
                coapResponseListener = BlockwiseCoapResponseListener(
                    request, emitter, isInitialized)
                responseForBlockwise(
                    selfPtr = thisPointerWrapper,
                    request = request,
                    responseListener = coapResponseListener
                )
            } else {
                coapResponseListener = SingleCoapResponseListener(
                    request, emitter)
                responseFor(
                    selfPtr = thisPointerWrapper,
                    request = request,
                    responseListener = coapResponseListener
                )
            }
            emitter.setCancellable {
                /**
                 * Only call native cancel method if request was added successfully (resultCode >= 0)
                 * and we have not already received complete response
                 */
                if (responseForResult.resultCode >= 0 &&
                    !coapResponseListener.isComplete() ) {
                    coapResponseListener.cleanupNativeListener()
                }
            }
        }.observeOn(scheduler)
    }

    override fun addResourceHandler(
        path: String,
        handler: ResourceHandler,
        configuration: CoapEndpointHandlerConfiguration
    ): Completable {
        return Completable.create { emitter ->
            synchronized(this) {
                if (resourceHandlerMap[path] != null) {
                    emitter.onError(CoapEndpointException(message = "Cannot register Handler for $path as it is already registered"))
                    return@create
                }
                val result = addResourceHandler(
                    selfPtr = thisPointerWrapper,
                    path = path,
                    handler = handler,
                    filterGroup = configuration.filterGroup.value

                )
                if (result.resultCode < 0) {
                    emitter.onError(
                        CoapEndpointException(
                            error = result.resultCode,
                            message = "Error registering resource handler for path: $path"
                        )
                    )
                } else {
                    resourceHandlerMap[path] = result.handlerNativeReference
                    emitter.onComplete()
                }
            }
        }
    }

    override fun removeResourceHandler(path: String): Completable = Completable.fromAction {
        synchronized(this) {
            resourceHandlerMap[path]?.let { handlerNativeReference ->
                val result = removeResourceHandler(handlerNativeReference)
                resourceHandlerMap.remove(path)
                Timber.i("Result for un-registering resource handler with path=$path: $result")
            }
        }
    }

    override fun attach(dataSinkDataSource: DataSinkDataSource) {
        if (this.dataSinkDataSource == null && thisPointerWrapper.isNotNull()) {
            this.dataSinkDataSource = dataSinkDataSource
            attach(
                selfPtr = thisPointerWrapper,
                sourcePtr = dataSinkDataSource.getAsDataSourcePointer(),
                sinkPtr = dataSinkDataSource.getAsDataSinkPointer()
            )
        } else {
            throw RuntimeException("Please detach before re-attaching")
        }
    }

    override fun detach() {
        this.dataSinkDataSource?.let {
            detach(
                selfPtr = thisPointerWrapper,
                sourcePtr = it.getAsDataSourcePointer()
            )
            this.dataSinkDataSource = null
        }
    }

    override fun getAsDataSinkPointer() = asDataSink()

    override fun getAsDataSourcePointer() = asDataSource()

    override fun setFilterGroup(group: CoapGroupRequestFilterMode, key: NodeKey<String>) {
        Timber.d("StackPeer with key $key sets filter group to $group")
        requestFilter.setGroupTo(group)
    }

    override fun close() {
        Timber.i("Closing CoapEndpoint")
        isInitialized.set(false)
        resourceHandlerMap.clear()
        requestFilter.close()
        detach()
        if (thisPointerWrapper.isNotNull()) {
            destroy(thisPointerWrapper)
        }
    }

    /**
     * Result for [CoapEndpoint.addResourceHandler] native call
     */
    class AddResourceHandlerResult(
        /** Result code for add resource handler request, -ve value indicates error */
        val resultCode: Int,
        /** Reference to native handler for single add resource request,
         * this reference is used for removing a handler */
        val handlerNativeReference: Long
    )

    /**
     * Result for [CoapEndpoint.responseFor] and [CoapEndpoint.responseForBlockwise] native call
     */
    class ResponseForResult(
        /** Result code for responseFor request, -ve value indicates error */
        val resultCode: Int,
        /** Reference to native response listener that holds reference to ongoing CoAP request
         * only valid when [resultCode]  is +ve */
        val nativeResponseListenerReference: Long
    )

    private external fun create(): Long

    private external fun attach(selfPtr: Long, sourcePtr: Long, sinkPtr: Long)

    private external fun detach(selfPtr: Long, sourcePtr: Long)

    private external fun asDataSource(selfPtr: Long = thisPointerWrapper): Long

    private external fun asDataSink(selfPtr: Long = thisPointerWrapper): Long

    private external fun destroy(selfPtr: Long)

    private external fun responseFor(
        selfPtr: Long,
        request: OutgoingRequest,
        responseListener: CoapResponseListener
    ): ResponseForResult

    private external fun responseForBlockwise(
        selfPtr: Long,
        request: OutgoingRequest,
        responseListener: CoapResponseListener
    ): ResponseForResult

    private external fun addResourceHandler(
        selfPtr: Long,
        path: String,
        handler: ResourceHandler,
        filterGroup: Byte
    ): AddResourceHandlerResult

    private external fun removeResourceHandler(handlerNativeReference: Long): Int

    private external fun attachFilter(selfPtr: Long, filterPrt: Long)
}
