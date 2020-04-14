package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.node.NodeMapper

/**
 * Utility object for instantiating a [StackNodeBuilder]<[CoapEndpoint], [BluetoothAddressNodeKey]>
 */
class CoapNodeProvider(
        private val nodeMapper: NodeMapper = NodeMapper.instance,
        private val stackNodeBuilder: StackNodeBuilder<CoapEndpoint> =
                StackNodeBuilder(CoapEndpoint::class.java, CoapEndpointBuilder, DtlsSocketNetifGattlink()),
        private val bluetoothAddressNodeKeyProvider: (String) -> BluetoothAddressNodeKey = { BluetoothAddressNodeKey(it) }
) {

    /**
     * @return a [StackNode]<[CoapEndpoint]>
     */
    fun getNode(bluetoothAddress: String) = nodeMapper.get(
            bluetoothAddressNodeKeyProvider(bluetoothAddress),
            stackNodeBuilder
    )

    /**
     * Removes a node from the mapper, this will close the StackNode
     *
     * @param bluetoothAddress the MAC address of the node we want to remove
     */
    fun removeNode(bluetoothAddress: String) = nodeMapper.removeNode( bluetoothAddressNodeKeyProvider(bluetoothAddress))
}
