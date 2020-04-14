package com.fitbit.goldengate.bindings.coap

import androidx.annotation.VisibleForTesting
import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.NativeReference
import com.fitbit.goldengate.bindings.coap.block.BlockwiseCoapResponseListener
import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequest
import com.fitbit.goldengate.bindings.coap.handler.ResourceHandler
import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import io.reactivex.Completable
import io.reactivex.Single
import timber.log.Timber
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Coap Client/Server API.
 *
 * Use [CoapEndpointProvider] for creating an instance
 */
class CoapEndpoint: NativeReference, StackService, Endpoint, DataSinkDataSource {

    override val thisPointer: Long

    internal var dataSinkDataSource: DataSinkDataSource? = null
    private val resourceHandlerMap = mutableMapOf<String, Long>()
    val requestFilter: CoapGroupRequestFilter = CoapGroupRequestFilter()
    private val initialized = AtomicBoolean()

    init {
        thisPointer = create()
        attachFilter(thisPointer, requestFilter.thisPointer)
        initialized.set(true)
    }

    override fun responseFor(request: OutgoingRequest): Single<IncomingResponse> {
        return Single.create<IncomingResponse> { emitter ->
            val coapResponseListener: CoapResponseListener
            val responseForResult = if (!request.forceNonBlockwise) {
                coapResponseListener = BlockwiseCoapResponseListener(request, emitter)
                responseForBlockwise(
                        selfPtr = thisPointer,
                        request = request,
                        responseListener = coapResponseListener
                )
            } else {
                coapResponseListener = SingleCoapResponseListener(request, emitter)
                responseFor(
                        selfPtr = thisPointer,
                        request = request,
                        responseListener = coapResponseListener
                )
            }
            emitter.setCancellable {
                /**
                 * Only call native cancel method if request was added successfully (resultCode >= 0)
                 * and we have not already received complete response
                 */
                if (initialized.get() && responseForResult.resultCode >= 0 && !coapResponseListener.isComplete()) {
                    if (!request.forceNonBlockwise) {
                        cancelResponseForBlockwise(responseForResult.nativeResponseListenerReference)
                    } else {
                        cancelResponseFor(responseForResult.nativeResponseListenerReference)
                    }
                }
            }
        }
    }

    override fun addResourceHandler(path: String, handler: ResourceHandler, configuration: CoapEndpointHandlerConfiguration): Completable {
        return Completable.create { emitter ->
            synchronized(this) {
                if (resourceHandlerMap[path] != null) {
                    emitter.onError(CoapEndpointException(message = "Cannot register Handler for $path as it is already registered"))
                    return@create
                }
                val result = addResourceHandler(
                        selfPtr = thisPointer,
                        path = path,
                        handler = handler,
                        filterGroup = configuration.filterGroup.value

                )
                if (result.resultCode < 0) {
                    emitter.onError(CoapEndpointException(
                            error = result.resultCode,
                            message = "Error registering resource handler for path: $path")
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
        if (this.dataSinkDataSource == null) {
            this.dataSinkDataSource = dataSinkDataSource
            attach(
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
                    selfPtr = thisPointer,
                    sourcePtr = it.getAsDataSourcePointer()
            )
            this.dataSinkDataSource = null
        }
    }

    override fun getAsDataSinkPointer() = asDataSink()

    override fun getAsDataSourcePointer() = asDataSource()

    override fun setFilterGroup(group: CoapGroupRequestFilterMode, key: NodeKey<String>) {
        Timber.d("StackNode with key $key sets filter group to $group")
        requestFilter.setGroupTo(group)
    }

    override fun close() {
        Timber.i("Closing CoapEndpoint")
        initialized.set(false)
        resourceHandlerMap.clear()
        requestFilter.close()
        detach()
        destroy()
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

    private external fun attach(selfPtr: Long = thisPointer, sourcePtr: Long, sinkPtr: Long)

    private external fun detach(selfPtr: Long = thisPointer, sourcePtr: Long)

    private external fun asDataSource(selfPtr: Long = thisPointer): Long

    private external fun asDataSink(selfPtr: Long = thisPointer): Long

    private external fun destroy(selfPtr: Long = thisPointer)

    private external fun responseFor(selfPtr: Long, request: OutgoingRequest, responseListener: CoapResponseListener): ResponseForResult

    private external fun cancelResponseFor(nativeResponseListenerReference: Long)

    private external fun responseForBlockwise(selfPtr: Long, request: OutgoingRequest, responseListener: CoapResponseListener): ResponseForResult

    private external fun cancelResponseForBlockwise(nativeResponseListenerReference: Long)

    private external fun addResourceHandler(selfPtr: Long, path: String, handler: ResourceHandler, filterGroup: Byte): AddResourceHandlerResult

    private external fun removeResourceHandler(handlerNativeReference: Long): Int

    private external fun attachFilter(selfPtr: Long, filterPrt: Long)
}
