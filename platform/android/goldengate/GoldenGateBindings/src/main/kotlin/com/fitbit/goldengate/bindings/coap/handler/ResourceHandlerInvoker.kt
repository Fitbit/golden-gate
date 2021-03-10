// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.handler

import android.os.SystemClock
import com.fitbit.goldengate.bindings.coap.data.Data
import com.fitbit.goldengate.bindings.coap.data.IncomingBody
import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import com.fitbit.goldengate.bindings.coap.data.RawRequestMessage
import io.reactivex.Observer
import io.reactivex.Single
import io.reactivex.internal.util.EmptyComponent
import timber.log.Timber
import java.util.concurrent.TimeUnit

/**
 * Helper method used in JNI server binding code to call appropriate requested method
 */
internal class ResourceHandlerInvoker(
        private val resourceHandler: ResourceHandler,
        private val elapsedRealtimeProvider: () -> Long = { SystemClock.elapsedRealtime() }
) {
    fun invoke(rawRequestMessage: RawRequestMessage): OutgoingResponse {
        val request = object : IncomingRequest {
            override val method: Method
                get() = rawRequestMessage.method
            override val options: Options
                get() = rawRequestMessage.options
            override val body: IncomingBody
                get() = object : IncomingBody {
                    override fun asData(): Single<Data> {
                        return Single.just(rawRequestMessage.data)
                    }
                }
            override val progressObserver: Observer<Int>
                // TODO for now progress is supported only for Outgoing Requests
                get() = EmptyComponent.asObserver()
        }

        val responseSingle = when (request.method) {
            Method.GET -> resourceHandler.onGet(request, OutgoingResponseBuilder())
            Method.PUT -> resourceHandler.onPut(request, OutgoingResponseBuilder())
            Method.POST -> resourceHandler.onPost(request, OutgoingResponseBuilder())
            Method.DELETE -> resourceHandler.onDelete(request, OutgoingResponseBuilder())
        }

        // TODO: making a blocking call right now but this should instead support sending response back to jni asynchronously
        val startTime = elapsedRealtimeProvider()
        val response = responseSingle.blockingGet()
        val timeBlocked = elapsedRealtimeProvider() - startTime
        if (timeBlocked > TimeUnit.MILLISECONDS.convert(1, TimeUnit.SECONDS)) {
            Timber.e("GG Thread blocked on handler response from ${resourceHandler.javaClass.name} for $timeBlocked milliseconds!")
        }
        return response
    }
}
