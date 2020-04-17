//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ConnectionRegistry.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 4/6/18.
//

import GoldenGate

/// Allows dynamic access to ConnectionControllers
protocol ConnectionRegistryType {
    /// Convert a Remote API UUID to a proper BluetoothPeerDescriptor.
    func descriptor(uuid: UUID) -> BluetoothPeerDescriptor

    /// Find a ConnectionController for a given descriptor.
    func find(descriptor: BluetoothPeerDescriptor) -> DefaultConnectionController?
}

/// Default implementation of ConnectionRegistryType
class ConnectionRegistry<Peer: ManagedPeer>: ConnectionRegistryType {
    private let peerManager: PeerManager<Peer>
    private let defaultRole: Role

    /// Creates a ConnectionRegistry.
    init(peerManager: PeerManager<Peer>, defaultRole: Role) {
        self.peerManager = peerManager
        self.defaultRole = defaultRole
    }

    func descriptor(uuid: UUID) -> BluetoothPeerDescriptor {
        return BluetoothPeerDescriptor(identifier: uuid)
    }

    func find(descriptor: BluetoothPeerDescriptor) -> DefaultConnectionController? {
        return peerManager.get(bluetoothPeerDescriptor: descriptor)?.connectionController
    }
}
