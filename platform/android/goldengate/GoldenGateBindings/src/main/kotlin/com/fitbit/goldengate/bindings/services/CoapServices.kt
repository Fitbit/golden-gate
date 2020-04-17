// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.services

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.remote.RemoteShellThread
import com.fitbit.goldengate.bindings.stack.StackService

class CoapServices private constructor(
    private val endpoint: CoapEndpoint,
    private val remoteShellThread: RemoteShellThread,
    generatorServiceProvider: (CoapEndpoint) -> CoapGeneratorService = { CoapGeneratorService.Provider(remoteShellThread).get(it) },
    testServiceProvider: (CoapEndpoint) -> CoapTestService = { CoapTestService.Provider(remoteShellThread).get(it) }
):StackService {

    private val generatorService:CoapGeneratorService = generatorServiceProvider(endpoint)
    private val testService:CoapTestService = testServiceProvider(endpoint)

    class Provider(private val remoteShellThread: RemoteShellThread) {
        fun get(endPoint: CoapEndpoint = CoapEndpointBuilder()) =
            CoapServices(endPoint, remoteShellThread)
    }

    override fun attach(dataSinkDataSource: DataSinkDataSource) {
        endpoint.attach(dataSinkDataSource)
    }

    override fun detach() {
        endpoint.detach()
    }

    override fun close() {
        generatorService.close()
        testService.close()
    }
}
