//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ManagedPeer.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import BluetoothConnection
import GoldenGate
import Rxbit
import RxCocoa
import RxSwift

/// Setup shared by "ManagedHub" and "ManagedNode" classes.
class ManagedPeer<ConnectionType: LinkConnection>: GGPeer, ManagedPeerType {
    /// ManagedPeer's `ConnectionController`.
    public let connectionController: ConnectionController<ConnectionType>

    /// The database record that is backing the peer.
    let record: PeerRecord

    /// The peer's name.
    let name: BehaviorRelay<String>

    private let disposeBag = DisposeBag()

    init(
        connectionController: ConnectionController<ConnectionType>,
        record: PeerRecord,
        peerParameters: PeerParameters,
        runLoop: GoldenGate.RunLoop,
        globalBlasterConfiguration: Observable<BlasterService.Configuration>
    ) {
        self.record = record
        self.name = BehaviorRelay(value: record.name)
        self.connectionController = connectionController

        let networkLink: Observable<NetworkLink?> = connectionController
            .connectionStatus
            .map { $0.connection }
            .flatMapLatestForwardingNil { $0.networkLink }
            .distinctUntilChanged(nilComparer)

        super.init(
            peerParameters,
            networkLink: networkLink,
            runLoop: runLoop,
            globalBlasterConfiguration: globalBlasterConfiguration
        )

        // Load the descriptor from the database into the Connection Controller.
        if let descriptor = record.peerDescriptor {
            connectionController.update(descriptor: descriptor)
        }

        // Clear `peerDescriptor` from DB
        // if the connection controller marked it as invalid.
        connectionController.descriptor
            .skip(1)
            .subscribe(onNext: {
                record.peerDescriptor = $0
            })
            .disposed(by: disposeBag)
    }
}
