//
//  HubComponent.swift
//  GoldenGateHost-iOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation
import GoldenGate

/// DI container for the Hub role
class HubComponent: ComponentBase {
    required init() {
        super.init(dependencies: HubComponentDependencies())
    }

    lazy var peerManager: PeerManager<ManagedNode> = {
        PeerManager<ManagedNode>(
            database: self.peerRecordDatabase,
            peerProvider: makeManagedNode
        )
    }()

    func makeManagedNode(record: PeerRecord) -> ManagedNode {
        return ManagedNode(
            record: record,
            commonPeerParameters: commonPeerParameters,
            remoteTestServer: remoteTestServer,
            linkConfigurationService: linkConfigurationService
        )
    }
}

private class HubComponentDependencies: ComponentBaseDependencies {
    let role = Role.hub

    lazy var peerRecordDatabaseStore: PeerRecordDatabaseStore = {
        return PeerRecordDatabaseUserDefaultsStore(
            userDefaults: UserDefaults.standard,
            key: "nodeRecordDatabase"
        )
    }()
}

/// Utility to use "Component.instance" everywere instead
/// of using the platform specific version.
class Component {
    static let instance = HubComponent()
}
