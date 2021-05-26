//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NodeComponent.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import BluetoothConnection
import Foundation
import GoldenGate
import RxSwift

/// DI container for the Node role
class NodeComponent: ComponentBase {
    required init() {
        super.init(dependencies: NodeComponentDependencies())
    }

    lazy var peerManager: PeerManager<ManagedHub> = {
        PeerManager<ManagedHub>(
            database: peerRecordDatabase,
            peerProvider: makeManagedHub
        )
    }()

    func makeManagedHub(record: PeerRecord) -> ManagedHub {
        return ManagedHub(
            connectionController: makeConnectionController(resolver: node),
            record: record,
            peerParameters: peerParameters,
            runLoop: runLoop,
            globalBlasterConfiguration: globalBlasterConfiguration.asObservable()
        )
    }

    lazy var nodeSimulator: NodeSimulator = {
        NodeSimulator(
            connectionController: makeConnectionController(resolver: node),
            advertiser: advertiser,
            peerManager: peerManager
        )
    }()
}

private class NodeComponentDependencies: ComponentBaseDependencies {
    let role = Role.node

    lazy var peerRecordDatabaseStore: PeerRecordDatabaseStore = {
        VoidPeerRecordDatabaseStore()
    }()
}

/// Utility to use "Component.instance" everywere instead
/// of using the platform specific version.
class Component {
    static let instance = NodeComponent()
}
