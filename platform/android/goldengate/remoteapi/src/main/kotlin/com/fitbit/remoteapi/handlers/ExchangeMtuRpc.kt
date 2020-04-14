package com.fitbit.remoteapi.handlers

import android.content.Context
import co.nstant.`in`.cbor.model.DataItem
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.Number
import co.nstant.`in`.cbor.model.UnicodeString
import com.fitbit.goldengate.bindings.remote.CborHandler
import com.fitbit.remoteapi.RemoteApiConfigurationState
import com.fitbit.remoteapi.StagedCborHandler

class ExchangeMtuRpc private constructor() {

    companion object Handler {

        operator fun invoke(
                context: Context,
                remoteApiConfigurationState: RemoteApiConfigurationState
        ): CborHandler = StagedCborHandler(
                "bt/exchange_mtu",
                StagedCborHandler.Stages(
                        Parser(),
                        Processor(context, remoteApiConfigurationState),
                        NullResponder()
                )
        )
    }

    class Parser : StagedCborHandler.Parser<Int> {

        private val argumentErrorMessage =
                "Must pass in an mtu number with the key 'mtu'"

        override fun parse(dataItems: List<DataItem>): Int {
            val paramsMap = dataItems.firstOrNull { it is Map } as Map?
            val mtu = (paramsMap?.get(UnicodeString("mtu")) as? Number)
                    ?.value
                    ?.toInt()
                    ?: throw IllegalArgumentException(argumentErrorMessage)
            return mtu
        }
    }

    class Processor(
        private val context: Context,
        private val remoteApiConfigurationState: RemoteApiConfigurationState
    ) : StagedCborHandler.Processor<Int, Int> {

        override fun process(parameterObject: Int): Int =
            remoteApiConfigurationState.getNode(context)
                    .requestMtu(parameterObject)
                    .let { parameterObject }
    }
}
