package com.fitbit.remoteapi

import android.content.Context
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.NodeMapper
import com.fitbit.goldengate.node.stack.RemoteApiStackNodeBuilder
import com.fitbit.goldengate.node.stack.StackNode
import com.fitbit.goldengate.node.stack.StackNodeBuilder
import io.reactivex.internal.disposables.DisposableContainer
import io.reactivex.schedulers.Schedulers
import timber.log.Timber

/**
 * State and utility class for remote api operations. This class holds a map for linking peer names
 * to their configuration and a reference to the last known connection. This allows for the legacy
 * 'no name' remote api operation and using peer names during all operations.
 *
 * @param bluetoothProvider the [BluetoothProvider] used to search for devices by peer name
 * @param disposableContainer the [DisposableContainer] used to keep reference to connection subscriptions
 * @param nodeMapper the [NodeMapper] used to hold references to the [StackNode]
 * @param defaultConfiguration the default configuration when no other is set for a node
 * @param configurationMap Maps peer names to stack node configurations ([StackNodeBuilder]s)
 * @param bluetoothAddressNodeKeyProvider Scans for the device by name to get the bt address for a BluetoothAddressNodeKey
 * @param lastConnectionHolder holds a reference to the most recently connected StackNode for when a peer param is not used in subsequent commands
 */
class RemoteApiConfigurationState(
        private val bluetoothProvider: BluetoothProvider,
        private val disposableContainer: DisposableContainer,
        private val nodeMapper: NodeMapper = NodeMapper.instance,
        private val defaultConfiguration: StackNodeBuilder<*> = RemoteApiStackNodeBuilder(
                CoapEndpoint::class.java,
                DtlsSocketNetifGattlink(),
                CoapEndpointBuilder
        ),
        private val configurationMap: MutableMap<String?, StackNodeBuilder<*>> = mutableMapOf(),
        private val bluetoothAddressNodeKeyProvider: (GattConnection) -> BluetoothAddressNodeKey = {
            BluetoothAddressNodeKey(it.device.address)
        },
        private val lastConnectionHolder: LastConnectionHolder = LastConnectionHolder()
) {

    /**
     * Sets a configuration map entry
     *
     * @param macAddress the BTLE name of the device or null
     * @param stackNodeBuilder a stack node builder including the [StackConfig] and [StackService]
     */
    fun setPeerConfiguration(macAddress: String?, stackNodeBuilder: StackNodeBuilder<*>) {
        configurationMap[macAddress] = stackNodeBuilder
    }

    /**
     * Returns a [StackNode] for a given peer name
     *
     * @param macAddress the BTLE mac address of the device
     * @return
     *   - If [macAddress] is non-null: a [StackNode] with a configuration previously set by
     *   [setPeerConfiguration], or the configuration set with no peer name if one was not set with
     *   this peer name, or the default if non of the former exist
     *   - If [macAddress] is null: the last connected [StackNode]
     *   - [IllegalStateException] if [peerName] and the last connected node are null
     */
    fun getNode(context: Context, macAddress: String? = null): StackNode<*> =
        //Asking for a node by peer name?
        macAddress?.let { address ->
            try {
                bluetoothAddressNodeKeyProvider(bluetoothProvider.bluetoothDevice(context, address))
            } catch (e: Exception) {
                throw IllegalArgumentException("Device not found", e)
            }.let {
                //Get the node configuration
                val nodeBuilder =
                //Start with trying to get a configuration set by peer name
                        configurationMap[address]
                        //If the configuration was not set for the peer name, see if one was
                        //set without a peer name
                                ?: configurationMap[null]
                                //No config yet? Use the default one.
                                ?: defaultConfiguration
                //Get the node from the NodeMapper using the bt address key
                nodeMapper.get(it, nodeBuilder) as StackNode<*>
            }
        }
        //Not using a peer name? Then we are expecting a previous connection
        ?: lastConnectionHolder.lastConnectedNode
        //Unhappy path, no peer name and no previous connection
        ?: throw IllegalStateException("Remote API is not connected yet: no peer name and no previous connection")

    /**
     * Connects to a device asynchronously using [Schedulers.io] by mac address and sets it as the
     * last connected node
     *
     * @param macAddress the BTLE name of the device to connect to
     */
    fun connect(context: Context, macAddress: String) =
        disposableContainer.add(
                getNode(context, macAddress)
                    .also { lastConnectionHolder.lastConnectedNode = it }
                    .connection()
                    .subscribeOn(Schedulers.io())
                    .subscribe(
                            { Timber.i("Connected to device: $macAddress") },
                            { Timber.e(it, "Error connecting to $macAddress via remote api") }
                    )
        )

    fun startPairing(context: Context, macAddress: String) =
            getNode(context, macAddress).also { lastConnectionHolder.lastConnectedNode = it }

    data class LastConnectionHolder(var lastConnectedNode: StackNode<*>? = null)
}
