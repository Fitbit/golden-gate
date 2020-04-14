package com.fitbit.remoteapi.handlers

import android.content.Context
import co.nstant.`in`.cbor.model.DataItem
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.UnicodeString
import com.fitbit.goldengate.bindings.remote.CborHandler
import com.fitbit.linkcontroller.LinkControllerProvider
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionMode
import com.fitbit.remoteapi.BluetoothProvider
import com.fitbit.remoteapi.RemoteApiConfigurationState
import com.fitbit.remoteapi.StagedCborHandler
import timber.log.Timber

/**
 * Handler used to remote call setup connection speed using Link Controller
 */
class ConnectionModeRpc private constructor() {

    companion object Handler {

        operator fun invoke(
            context: Context,
            linkControllerProvider: LinkControllerProvider,
            bluetoothProvider: BluetoothProvider,
            remoteApiConfigurationState: RemoteApiConfigurationState
        ): CborHandler {
            return StagedCborHandler(
                "bt/connection_service/set_connection_speed",
                StagedCborHandler.Stages(
                    Parser(),
                    Processor(context, linkControllerProvider, bluetoothProvider, remoteApiConfigurationState),
                    NullResponder()
                )
            )
        }
    }

    /**
     * This handles the arguments of the set connection speed method from the connectionService.
     * It requires two parameters:
     * Peer -> the device you want to setup the connection speed
     * Speed -> the preferred connection speed
     */
    class Parser : StagedCborHandler.Parser<String> {
        enum class Params(val paramName: String) {
            SPEED("speed")
        }

        private val argumentErrorMessage =
            "Must pass in a target device name with the keys '${Params.SPEED.paramName}'"

        override fun parse(dataItems: List<DataItem>): String {
            //request params has one param: "name" which is a String representing the BTLE name of a tracker to connect to.
            val paramsMap = dataItems.firstOrNull { it is Map } as Map?
                ?: throw IllegalArgumentException(argumentErrorMessage)

            val speed =
                (paramsMap.get(UnicodeString(Params.SPEED.paramName)) as? UnicodeString)?.string
                    ?: throw IllegalArgumentException(argumentErrorMessage)
            return speed

        }
    }

    /**
     * This processes the command received over remote api.
     * It will obtain a BluetoothDevice from the first parameter and using the [LinkController]
     * it will set a preferred connection speed by using the second parameter.
     */
    class Processor(
        private val context: Context,
        private val linkControllerModule: LinkControllerProvider,
        private val bluetoothProvider: BluetoothProvider,
        private val remoteApiConfigurationState: RemoteApiConfigurationState
    ) : StagedCborHandler.Processor<String, String> {

        override fun process(parameterObject: String): String {
            val macAddress = remoteApiConfigurationState.getNode(context).key.value
            val gattConnection = try {
                bluetoothProvider.bluetoothDevice(context, macAddress)
            } catch (e: Exception) {
                throw IllegalArgumentException("Device not found")
            }
            val connectionSpeed = try {
                PreferredConnectionMode.valueOf(parameterObject.toUpperCase())
            } catch (e: Exception){
                throw java.lang.IllegalArgumentException("Speed parameter not valid. Needs to be either fast or slow")
            }
            linkControllerModule.getLinkController(gattConnection)
                .setPreferredConnectionMode(connectionSpeed)
                .blockingAwait()

            Timber.d("Set connection mode to ${parameterObject} for Device: ${macAddress}")
            return gattConnection.device.name
        }
    }

}
