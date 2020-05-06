//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ManagedPeer.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import GoldenGate
import RxCocoa
import RxSwift

/// Setup shared by "ManagedHub" and "ManagedNode" classes.
class ManagedPeer: CommonPeer, ManagedPeerType {
    /// The database record that is backing the peer.
    let record: PeerRecord

    /// The peer's name.
    let name: BehaviorRelay<String>

    private let disposeBag = DisposeBag()

    init(record: PeerRecord, commonPeerParameters: CommonPeer.Parameters) {
        self.record = record
        self.name = BehaviorRelay(value: record.name)

        super.init(commonPeerParameters)

        // Load the descriptor from the database into the Connection Controller.
        if let descriptor = record.bluetoothPeerDescriptor {
            connectionController.update(descriptor: descriptor)
        }

        // Clear `bluetoothPeerDescriptor` from DB
        // if the connection controller marked it as invalid.
        connectionController.descriptor
            .skip(1)
            .subscribe(onNext: {
                record.bluetoothPeerDescriptor = $0
            })
            .disposed(by: disposeBag)

        setUserWantsToConnect(true)
    }
}
