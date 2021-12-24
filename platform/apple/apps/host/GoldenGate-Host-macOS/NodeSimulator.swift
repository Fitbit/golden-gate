//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NodeSimulator.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 11/18/17.
//

import BluetoothConnection
import Foundation
import GoldenGate
import RxCocoa
import RxSwift

// swiftlint:disable no_print

/// Test-bed for emulating a node using your macOS device.
class NodeSimulator {
    let connectionController: ConnectionController<NodeConnection>
    let advertiser: BluetoothAdvertiser

    private let peerManager: PeerManager<ManagedHub>
    private let scheduler: SerialDispatchQueueScheduler
    private let disposeBag = DisposeBag()

    init(
        connectionController: ConnectionController<NodeConnection>,
        advertiser: BluetoothAdvertiser,
        peerManager: PeerManager<ManagedHub>
    ) {
        self.connectionController = connectionController
        self.advertiser = advertiser

        self.peerManager = peerManager
        self.scheduler = SerialDispatchQueueScheduler(qos: .default)
    }

    func start() {
        /// Advertise so that the mobile device can find us.
        advertiser.advertise()
            .subscribe()
            .disposed(by: disposeBag)

        advertiser.isAdvertising
            .do(onNext: { Log("isAdvertising: \($0)", .info) })
            .subscribe()
            .disposed(by: disposeBag)

        // Whenever a peer (descriptor) subscribes to us
        // create a new `Hub` object (or force a reconnect).
        advertiser.subscribedPeerDescriptor
            .observe(on: scheduler)
            .map { [peerManager] descriptor -> ManagedHub in
                if let peer = peerManager.get(peerDescriptor: descriptor) {
                    return peer
                } else {
                    return peerManager.getOrCreate(peerDescriptor: descriptor, name: "Peer")
                }
            }
            .subscribe(onNext: { $0.connectionController.establishConnection(trigger: "node_connect") })
            .disposed(by: disposeBag)

        // Release `Hub` whenever the previously subscribed peer unsubscribed
        advertiser.unsubscribedPeerDescriptor
            .observe(on: scheduler)
            .compactMap(peerManager.get(peerDescriptor:))
            .do(onNext: peerManager.remove)
            .subscribe()
            .disposed(by: disposeBag)
    }
}
