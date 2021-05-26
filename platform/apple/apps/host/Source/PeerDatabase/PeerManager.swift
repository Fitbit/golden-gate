//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeerManager.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 11/3/17.
//

import BluetoothConnection
import Foundation
import RxCocoa
import RxSwift

/// A generic peer that is managed by the PeerManager.
protocol ManagedPeerType {
    /// The record representation of the peer.
    var record: PeerRecord { get }
}

/// Manages multiple peers and loads/stores them into a database.
class PeerManager<Peer: ManagedPeerType> {
    private let database: PeerRecordDatabase
    private let peerProvider: (PeerRecord) -> Peer

    private let disposeBag = DisposeBag()
    private(set) var peers = BehaviorRelay(value: [Peer]())
    private var peerByHandle = [PeerRecordHandle: Peer]()

    /// Crates a new PeerManager.
    init(
        database: PeerRecordDatabase,
        peerProvider: @escaping (PeerRecord) -> Peer
    ) {
        self.database = database
        self.peerProvider = peerProvider

        database.records.asObservable()
            .take(1)
            .flatMap { Observable.from($0) }
            .do(onNext: { [unowned self] in _ = self.ensurePeer(for: $0) })
            .subscribe()
            .disposed(by: disposeBag)
    }

    /// Removes a peer.
    func remove(_ peer: Peer) {
        let handle = peer.record.handle

        if peerByHandle.removeValue(forKey: handle) != nil {
            database.delete(peer.record)
            peers.accept(peers.value.filter { $0.record.handle != handle })
        }
    }
}

extension PeerManager {
    /// Retrieves a peer for a given peer descriptor.
    func get(peerDescriptor: PeerDescriptor) -> Peer? {
        return peers.value.first(where: { $0.record.peerDescriptor == peerDescriptor })
    }

    /// Retrieves or creates a peer for a given peer descriptor.
    func getOrCreate(peerDescriptor: PeerDescriptor, name: String) -> Peer {
        if let peer = get(peerDescriptor: peerDescriptor) {
            return peer
        }

        let record = database.create(name: name)
        record.peerDescriptor = peerDescriptor
        return ensurePeer(for: record)
    }
}

private extension PeerManager {
    func ensurePeer(for record: PeerRecord) -> Peer {
        if let peer = peerByHandle[record.handle] {
            return peer
        }

        let peer = peerProvider(record)

        peerByHandle[record.handle] = peer

        var newPeers = self.peers.value
        newPeers.append(peer)
        self.peers.accept(newPeers)

        return peer
    }
}
