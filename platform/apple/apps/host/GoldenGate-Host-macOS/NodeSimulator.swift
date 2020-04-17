//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NodeSimulator.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 11/18/17.
//

import Foundation
import GoldenGate
import RxCocoa
import RxSwift

// swiftlint:disable no_print

/// Test-bed for emulating a node using your macOS device.
class NodeSimulator {
    let node: Node
    let defaultConnectionControllerProvider: (Observable<StackDescriptor>) -> DefaultConnectionController
    let advertiser: Node.Advertiser

    private let peerManager: PeerManager<ManagedHub>
    private let scheduler: SerialDispatchQueueScheduler
    private let disposeBag = DisposeBag()

    init(
        node: Node,
        defaultConnectionControllerProvider: @escaping (Observable<StackDescriptor>) -> DefaultConnectionController,
        advertiser: Node.Advertiser,
        peerManager: PeerManager<ManagedHub>
    ) {
        self.node = node
        self.defaultConnectionControllerProvider = defaultConnectionControllerProvider
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
        advertiser.subscribedBluetoothPeerDescriptor
            .observeOn(scheduler)
            .map { [peerManager] descriptor -> ManagedHub in
                if let peer = peerManager.get(bluetoothPeerDescriptor: descriptor) {
                    return peer
                } else {
                    return peerManager.getOrCreate(bluetoothPeerDescriptor: descriptor, name: "Peer")
                }
            }
            .map { $0.setUserWantsToConnect(true) }
            .subscribe()
            .disposed(by: disposeBag)

        // Release `Hub` whenever the previously subscribed peer unsubscribed
        advertiser.unsubscribedBluetoothPeerDescriptor
            .observeOn(scheduler)
            .map(peerManager.get(bluetoothPeerDescriptor:))
            .filterNil()
            .do(onNext: peerManager.remove)
            .subscribe()
            .disposed(by: disposeBag)

        if let server = Component.instance.remoteTestServer {
            register(on: server)
            server.start()
        }
    }

    func register(on server: RemoteTestServerType) {
        server.register(module: AdvertisementRemoteApi(
            advertiser: advertiser
        ))
    }
}
