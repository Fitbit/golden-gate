// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi.handlers

import android.content.Context
import co.nstant.`in`.cbor.CborBuilder
import co.nstant.`in`.cbor.CborEncoder
import com.fitbit.goldengate.bindings.remote.CborHandler
import com.fitbit.remoteapi.RemoteApiConfigurationState
import com.fitbit.remoteapi.StagedCborHandler
import java.io.ByteArrayOutputStream

class ConnectRpc private constructor() {

    companion object Handler {

        operator fun invoke(
            context: Context,
            remoteApiConfigurationState: RemoteApiConfigurationState
        ): CborHandler = StagedCborHandler(
                "bt/connect",
                StagedCborHandler.Stages(
                        StagedCborHandler.UnitParser(),
                        Processor(context, remoteApiConfigurationState),
                        Responder()
                )
        )
    }

    class Processor(
            private val context: Context,
            private val remoteApiConfigurationState: RemoteApiConfigurationState
    ) : StagedCborHandler.Processor<Unit, String> {

        override fun process(parameterObject: Unit): String {
            val macAddress = remoteApiConfigurationState.getNode(context).key.value
            remoteApiConfigurationState.connect(context, macAddress)
            return macAddress
        }
    }

    class Responder : StagedCborHandler.Responder<String> {

        override fun respondWith(responseObject: String): ByteArray {
            val cborReturnValueStream = ByteArrayOutputStream()
            val encoder = CborEncoder(cborReturnValueStream)
            val builder = CborBuilder()
            val builtStructure = builder
                    .add(responseObject)
                    .build()
            encoder.encode(builtStructure)
            return cborReturnValueStream.toByteArray()
        }
    }
}
