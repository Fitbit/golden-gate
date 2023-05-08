// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.stackservice

import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.node.stack.StackPeer
import com.fitbit.goldengate.node.stack.StackPeerBuilder

/**
 * Utility object for encapsulating dependencies. When [invoked][invoke] returns a
 * [StackServiceMapper]<[CoapEndpoint], [BluetoothAddressNodeKey]> using a
 * [StackPeerBuilder]<[CoapEndpoint]>
 */
object CoapStackServiceProvider {

  /**
   * @return a [StackServiceMapper]<[CoapEndpoint], [String]> using a
   *   [StackPeerBuilder]<[CoapEndpoint]>
   */
  operator fun invoke(
    shouldSetStartMtuChecker: () -> Boolean = { true },
    peerRole: PeerRole = PeerRole.Peripheral,
    stackConfig: StackConfig = DtlsSocketNetifGattlink(),
    stackPeerBuilder: StackPeerBuilder<CoapEndpoint> =
      StackPeerBuilder(
        CoapEndpoint::class.java,
        peerRole,
        CoapEndpointBuilder,
        stackConfig,
        shouldSetStartMtuChecker
      )
  ): StackServiceMapper<CoapEndpoint, BluetoothAddressNodeKey> =
    StackServiceMapper({ (it as StackPeer).stackService as CoapEndpoint }, stackPeerBuilder)
}
