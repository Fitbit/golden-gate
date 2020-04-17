// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi.handlers

import co.nstant.`in`.cbor.model.DataItem
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.UnicodeString
import com.fitbit.goldengate.bindings.remote.CborHandler
import com.fitbit.goldengate.bindings.services.BlastService
import com.fitbit.goldengate.bindings.services.CoapServices
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.GattlinkStackConfig
import com.fitbit.goldengate.bindings.stack.SocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.stack.RemoteApiStackNodeBuilder
import com.fitbit.goldengate.node.stack.StackNodeBuilder
import com.fitbit.remoteapi.RemoteApiConfigurationState
import com.fitbit.remoteapi.StagedCborHandler

class ConfigurationRpc private constructor() {

    companion object Handler {

        //Stack configs
        const val gattlinkConfig = "gattlink"
        const val udpConfig = "udp"
        const val dtlsConfig = "dtls"

        //Stack Services
        const val blastService = "blast"
        const val coapService = "coap"

        operator fun invoke(
            remoteApiState: RemoteApiConfigurationState,
            coapServicesProvider: CoapServices.Provider,
            blastServiceProvider: BlastService.Provider
        ): CborHandler = StagedCborHandler(
                "stack/set_type",
                StagedCborHandler.Stages(
                        Parser(coapServicesProvider, blastServiceProvider),
                        Processor(remoteApiState),
                        NullResponder()
                )
        )
    }

    class Parser(
        private val coapServicesProvider: CoapServices.Provider,
        private val blastServiceProvider: BlastService.Provider
    ) : StagedCborHandler.Parser<StackNodeBuilder<out StackService>> {

        enum class Params(val paramName: String) {
            STACK_TYPE("stack_type"),
            SERVICE("service")
        }

        private val stackConfigArgumentErrorMessage =
                "Must pass in a stack type name with the key '${Params.STACK_TYPE.paramName}' and must be '$gattlinkConfig', '$udpConfig', or '$dtlsConfig'"

        private val stackServiceArgumentErrorMessage =
                "Must pass in a stack service name with the key '${Params.SERVICE.paramName}' and must be '$blastService', '$coapService'"

        override fun parse(dataItems: List<DataItem>): StackNodeBuilder<out StackService> {
            val paramsMap = dataItems.firstOrNull { it is Map } as Map?
                    ?: throw IllegalArgumentException("$stackConfigArgumentErrorMessage\n$stackServiceArgumentErrorMessage")
            val stackConfig =
                    (paramsMap.get(UnicodeString(Params.STACK_TYPE.paramName)) as? UnicodeString)?.string.let {
                        when (it) {
                            gattlinkConfig -> GattlinkStackConfig
                            udpConfig -> SocketNetifGattlink()
                            dtlsConfig -> DtlsSocketNetifGattlink()
                            else -> throw IllegalArgumentException(stackConfigArgumentErrorMessage)
                        }
                    }

            return (paramsMap.get(UnicodeString(Params.SERVICE.paramName)) as? UnicodeString)?.string.let {
                when (it) {
                    blastService -> blasterStackNodeBuilder(stackConfig)
                    coapService -> coapStackNodeBuilder(stackConfig)
                    else -> throw IllegalArgumentException(stackServiceArgumentErrorMessage)
                }
            }
        }

        private fun blasterStackNodeBuilder(stackConfig: StackConfig): StackNodeBuilder<BlastService> =
            RemoteApiStackNodeBuilder(BlastService::class.java, stackConfig) { blastServiceProvider.get() }

        private fun coapStackNodeBuilder(stackConfig: StackConfig) =
            RemoteApiStackNodeBuilder(CoapServices::class.java, stackConfig) { coapServicesProvider.get() } }

    class Processor(private val remoteApiConfigurationState: RemoteApiConfigurationState) : StagedCborHandler.Processor<StackNodeBuilder<out StackService>, Unit> {

        override fun process(parameterObject: StackNodeBuilder<out StackService>) {
            remoteApiConfigurationState.setPeerConfiguration(null, parameterObject)
        }
    }
}
