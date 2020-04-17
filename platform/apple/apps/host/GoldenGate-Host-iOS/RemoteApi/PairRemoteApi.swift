//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PairRemoteApi.swift
//  GoldenGateHost-iOS
//
//  Created by Vlad-Mihai Corneci on 27/02/2019.
//

import GoldenGate
import GoldenGateXP
import RxCocoa
import RxSwift

enum PairRemoteAPIError: Error {
    case invalidType
    case invalidPairedDevice(String)
}

class PairRemoteApi: RemoteApiModule {
    let peerManager: PeerManager<ManagedNode>
    let scanner: Hub.Scanner

    private let startPairingSubject = PublishSubject<PairingParams>()
    private let disposeBag = DisposeBag()

    public let methods: Set<String> = Method.allUniqueRawValues

    init (peerManager: PeerManager<ManagedNode>, scanner: Hub.Scanner) {
        self.peerManager = peerManager
        self.scanner = scanner

        startPairingSubject
            .filter(validatePairingParams(_:))
            .flatMapLatest { params -> Observable<DefaultConnectionController> in
                let name = params.peer.id

                return Observable.concat(
                    self.connectionControllerForKnownTracker(with: name),
                    self.connectionControllerForNewDiscoveredTracker(with: name)
                )
                .take(1)
            }
            .do(onNext: { $0.triggerConnect() })
            .subscribe()
            .disposed(by: disposeBag)
    }

    private func validatePairingParams(_ params: PairingParams) throws -> Bool {
        guard params.peer.type == "name" else {
            throw PairRemoteAPIError.invalidType
        }

        return true
    }

    func publishHandlers(on remoteShell: RemoteShell) {
        remoteShell.register(Method.getState.rawValue, handler: getState)
        remoteShell.register(Method.getPairedDevices.rawValue, handler: getPairedDevices)
        remoteShell.register(Method.startPairing.rawValue, handler: startPairing)
    }

    /// Starts the pairing process with a device identified by a peerId.
    ///
    /// - Parameter params: Encapsulated peerId.
    /// - Returns: An observable that will trigger the pairing process.
    private func startPairing(params: PairingParams) -> Completable {
        return Completable.deferred { [startPairingSubject]  in
            startPairingSubject.onNext(params)

            return .empty()
        }
    }

    /// Returns the current state of the pairing process.
    ///
    /// - Returns: The current state of the pairing process.
    private func getState() -> Single<State> {
        return .just(.idle)
    }

    /// Returns the list of all of the devices that are currently paired with
    /// the mobile app.
    ///
    /// - Note: The current version of RemoteAPI enforces the app to be paired
    ///         with maximum one device.
    ///
    /// - Returns: An observable that will emit a list of peer identifiers.
    private func getPairedDevices() -> Single<[PeerId]> {
        return peerManager.peers
            .take(1)
            .map { managedPeers in
                managedPeers.map { PeerId(id: $0.name.value, type: "name") }
            }
            .asSingle()
    }

    /// Look in the database for a tracker with the specified name.
    /// The method will initiate a connect.
    ///
    /// - Parameter name: The name of the desired tracker.
    /// - Returns: An observable that will emit the tracker's connection
    ///            controller or it will complete if the tracker does not exist.
    private func connectionControllerForKnownTracker(
        with name: String
    ) -> Observable<DefaultConnectionController> {
        return peerManager.peers.asObservable()
            .take(1)
            .flatMapLatest { Observable.from($0) }
            .filter { $0.record.name == name }
            .map { $0.connectionController }
            .take(1)
    }

    /// Discover a new tracker with the specified name.
    ///
    /// - Parameter name: The name of the desired peripheral.
    /// - Returns: An observable that will emit the connection controller of the
    ///            newly discovered tracker.
    private func connectionControllerForNewDiscoveredTracker(
        with name: String
    ) -> Observable<DefaultConnectionController> {
        return self.scanner.peers
            .flatMap { return Observable.from($0) }
            .filter { $0.name.value == name }
            .map { peer in
                let descriptor = BluetoothPeerDescriptor(
                    identifier: peer.identifier
                )

                return self.peerManager.getOrCreate(
                    bluetoothPeerDescriptor: descriptor,
                    name: peer.name.value
                ).connectionController
            }
            .take(1)
    }
}

private extension PairRemoteApi {
    enum Method: String, CaseIterable {
        case getState           = "pair/get_state"
        case getPairedDevices   = "pair/get_paired_devices"
        case startPairing       = "pair/start_pairing"
    }
}

private extension ReconnectingConnectionController {
    func triggerConnect() {
        _ = establishConnection().subscribe()
    }
}

struct PeerId: Codable {
    /// ID of the peer device specified by "type" below.
    let id: String

    /// Type of the ID provided
    let type: String
}

private struct PairingParams: Decodable {
    let peer: PeerId
}

private enum State: Int, Encodable {
    case idle = 0
}
