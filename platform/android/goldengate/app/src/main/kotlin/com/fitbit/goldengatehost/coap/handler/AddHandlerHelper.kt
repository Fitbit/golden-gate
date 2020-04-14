package com.fitbit.goldengatehost.coap.handler

import com.fitbit.goldengate.bindings.coap.CoapEndpointHandlerConfiguration
import com.fitbit.goldengate.bindings.coap.CoapGroupRequestFilterMode
import com.fitbit.goldengate.bindings.coap.Endpoint
import com.fitbit.goldengate.bindings.coap.handler.EchoResourceHandler
import com.fitbit.goldengate.bindings.coap.handler.ResourceHandler
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.disposables.Disposable
import io.reactivex.schedulers.Schedulers
import timber.log.Timber

fun addAllAvailableCoapHandlers(
        disposeBag: CompositeDisposable,
        endpoint: Endpoint
) {
    disposeBag.add(addResourceHandler(endpoint, "helloworld", HelloWorldResourceHandler()))
    disposeBag.add(addResourceHandler(endpoint, "echo", EchoResourceHandler()))
}

fun addResourceHandler(
        endpoint: Endpoint,
        path: String,
        resourceHandler: ResourceHandler
): Disposable {
    return endpoint.addResourceHandler(path, resourceHandler, CoapEndpointHandlerConfiguration(CoapGroupRequestFilterMode.GROUP_1))
            .subscribeOn(Schedulers.io())
            .subscribe(
                    { Timber.i("$String completed") },
                    { throwable -> Timber.e(throwable, "Error adding handler at path: $path") }
            )
}

fun removeAllAvailableCoapHandlers(
    endpoint: Endpoint
) {
    removeResourceHandler(endpoint, "helloworld")
    removeResourceHandler(endpoint, "echo")
}

fun removeResourceHandler(
    endpoint: Endpoint,
    path: String
)
        : Disposable {
    return endpoint.removeResourceHandler(path)
        .subscribeOn(Schedulers.io())
        .subscribe(
            { Timber.i("Removed Handler at path: $path") },
            { throwable -> Timber.e(throwable, "Error removing handler at path: $path") }
        )
}
