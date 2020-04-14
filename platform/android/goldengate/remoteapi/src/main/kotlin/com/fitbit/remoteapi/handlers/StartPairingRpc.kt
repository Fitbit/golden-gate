package com.fitbit.remoteapi.handlers

import android.content.Context
import co.nstant.`in`.cbor.model.DataItem
import co.nstant.`in`.cbor.model.Map
import com.fitbit.goldengate.bindings.remote.CborHandler
import com.fitbit.remoteapi.PeerId
import com.fitbit.remoteapi.PeerIdType
import com.fitbit.remoteapi.RemoteApiConfigurationState
import com.fitbit.remoteapi.StagedCborHandler

/**
 * Remote API handler for start pairing. This will create the connection and store it so other
 * handlers know which connection to use.
 */
class StartPairingRpc private constructor() {

    companion object Handler {
        operator fun invoke(
            context: Context,
            remoteApiConfigurationState: RemoteApiConfigurationState
        ): CborHandler = StagedCborHandler(
            "pair/start_pairing",
            StagedCborHandler.Stages(
                Parser(),
                Processor(context, remoteApiConfigurationState),
                NullResponder()
            )
        )
    }

    class Parser:StagedCborHandler.Parser<PeerId> {
        override fun parse(dataItems: List<DataItem>): PeerId {
            val paramMap = dataItems.firstOrNull { it is Map } as? Map
            val peerId = RemoteApiParserUtil.getPeerId(paramMap)

            if (peerId.type != PeerIdType.MAC_ADDRESS) throw IllegalStateException("No mac address specified")

            return peerId
        }
    }

    class Processor(val context: Context,
                    val remoteApiConfigurationState: RemoteApiConfigurationState):StagedCborHandler.Processor<PeerId, Unit> {
        override fun process(parameterObject: PeerId) {
            remoteApiConfigurationState.startPairing(context, parameterObject.id)
        }
    }

}