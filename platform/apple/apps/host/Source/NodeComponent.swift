//
//  NodeComponent.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

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
            record: record,
            commonPeerParameters: commonPeerParameters,
            remoteTestServer: remoteTestServer
        )
    }

    lazy var nodeSimulator: NodeSimulator = {
        NodeSimulator(
            node: node,
            defaultConnectionControllerProvider: makeDefaultConnectionController,
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
