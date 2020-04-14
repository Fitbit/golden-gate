package com.fitbit.goldengate.stackservice

import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.node.stack.StackNode
import com.fitbit.goldengate.node.stack.StackNodeBuilder

/**
 * Utility object for encapsulating dependencies. When [invoked][invoke] returns a
 * [StackServiceMapper]<[CoapEndpoint], [BluetoothAddressNodeKey]> using a [StackNodeBuilder]<[CoapEndpoint]>
 */
object CoapStackServiceProvider {

    /**
     * @return a [StackServiceMapper]<[CoapEndpoint], [String]> using a [StackNodeBuilder]<[CoapEndpoint]>
     */
    operator fun invoke(
        stackConfig: StackConfig = DtlsSocketNetifGattlink(),
        stackNodeBuilder: StackNodeBuilder<CoapEndpoint> =
            StackNodeBuilder(CoapEndpoint::class.java, CoapEndpointBuilder, stackConfig)
    ) : StackServiceMapper<CoapEndpoint, BluetoothAddressNodeKey> = StackServiceMapper(
            { (it as StackNode).stackService as CoapEndpoint },
            stackNodeBuilder
    )
}
