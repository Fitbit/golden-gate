package com.fitbit.remoteapi

import co.nstant.`in`.cbor.CborDecoder
import co.nstant.`in`.cbor.model.DataItem
import com.fitbit.goldengate.bindings.remote.CborHandler
import timber.log.Timber

class StagedCborHandler(override val name: String, val stages : Stages<*, *>) : CborHandler {

    /**
         * @return a ByteArray containing the result
         * @throws IllegalArgumentException if the requestParams do not conform to expectations or there is an internal error
         */
    override fun handle(requestParamsCbor: ByteArray) : ByteArray {
        val dataItems = CborDecoder.decode(requestParamsCbor)
        Timber.i("RemoteApi received request. Request Name: $name Params: $dataItems")
        try {
            return stages(dataItems)
        } catch (e: RuntimeException) {
            //Log before re-throwing so we don't have to on the native side
            Timber.e(e, "Exception while processing Remote API command")
            throw e
        }
    }

    data class Stages<P, R>(
            private val parser: Parser<P>,
            private val processor: Processor<P, R>,
            private val responder: Responder<R>
    ) {
        internal operator fun invoke(requestParamsCbor: List<DataItem>): ByteArray {
            val parameters = parser.parse(requestParamsCbor)
            val response = processor.process(parameters)
            return responder.respondWith(response)
        }
    }

    class UnitParser : Parser<Unit> {
        override fun parse(dataItems: List<DataItem>) {}
    }

    interface Parser<P> {

        /**
         * @return a parameter object parsed from the [dataItems]
         * @throws IllegalArgumentException if the requestParams do not conform to expectations
         */
        fun parse(dataItems: List<DataItem>) : P
    }

    interface Processor<P, R> {

        /**
         * Processes a request given a [parameterObject]
         *
         * @return a respondWith object
         * @throws IllegalArgumentException if there is an internal error
         */
        fun process(parameterObject: P) : R
    }

    /**
     * @return a ByteArray containing a Cbor encoded respondWith for the given respondWith object
     */
    interface Responder<R> {
        fun respondWith(responseObject: R) : ByteArray
    }
}
