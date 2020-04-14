//
//  ConnectionRemoteApi.swift
//  GoldenGateHost
//
//  Created by Bogdan Vlad on 12/15/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import GoldenGate
import GoldenGateXP
import RxSwift

enum ConnectionRemoteApiError: Error {
    case noDeviceFound
    case notConnected
    case noConnectionConfiguration
    case invalidConnectionController
    case unknown
}

class ConnectionRemoteApi: RemoteApiModule {
    let peerManager: PeerManager<ManagedNode>
    let scanner: Hub.Scanner
    private let disposeBag = DisposeBag()

    public let methods: Set<String> = Method.allUniqueRawValues

    init (peerManager: PeerManager<ManagedNode>, scanner: Hub.Scanner) {
        self.peerManager = peerManager
        self.scanner = scanner
    }

    func publishHandlers(on remoteShell: RemoteShell) {
        remoteShell.register(Method.connect.rawValue, handler: connect)
        remoteShell.register(Method.getConnectionConfiguration.rawValue, handler: getConfiguration)
        remoteShell.register(Method.getConnectionServiceStatus.rawValue, handler: getStatus)
        remoteShell.register(Method.disconnect.rawValue, handler: disconnect)
    }

    /// Triggers a connection to a previously paired device.
    private func connect() -> Completable {
        return peerManager.pairedDevice()
            .map { $0.connectionController }
            .do(onSuccess: { $0.triggerConnect() })
            .asCompletable()
    }

    private func getConfiguration() throws -> Single<GetConfigurationResult> {
        return peerManager.pairedDevice()
            .asObservable()
            .map { $0.connectionController }
            .flatMapLatest { $0.connection }
            .map { connection -> LinkStatusService.ConnectionConfiguration? in
                guard let connection = connection else {
                    throw ConnectionRemoteApiError.notConnected
                }

                return (connection as? Hub.Connection)?.remoteConnectionConfiguration.value
            }
            .map { configuration in
                guard let configuration = configuration else {
                    throw ConnectionRemoteApiError.noConnectionConfiguration
                }

                return GetConfigurationResult(
                    connectionInterval: configuration.connectionInterval,
                    slaveLatency: configuration.slaveLatency,
                    supervisionTimeout: configuration.supervisionTimeout,
                    MTU: configuration.MTU,
                    mode: String(describing: configuration.mode)
                )
            }
            .take(1)
            .asSingle()
    }

    private func getStatus(params: ConnectParams) throws -> Single<GetStatusResult> {
        return Single.deferred { [peerManager] in
            return peerManager.pairedDevice()
                .asObservable()
                .map { $0.connectionController }
                .flatMapLatest { $0.connection }
                .take(1)
                .map { connection in
                    guard let hubConnection = connection as? Hub.Connection? else {
                        throw ConnectionRemoteApiError.invalidConnectionController
                    }

                    let status: String
                    switch hubConnection?.networkLinkStatus.value {
                    case .none,
                         .disconnected?,
                         .connecting?:
                        status = "Disconnected"
                    case .negotiating?:
                        status = "Connected"
                    case .connected?:
                        status = "CommsReady"
                    }

                    return GetStatusResult(
                        bondedFlag: hubConnection?.remoteConnectionStatus.value?.bonded ?? false,
                        encryptedFlag: hubConnection?.remoteConnectionStatus.value?.encrypted ?? false,
                        connectionServiceState: status
                    )
                }
                .asSingle()
        }
    }

    private func disconnect() -> Completable {
        return peerManager.pairedDevice()
            .map { $0.connectionController.forceDisconnect() }
            .asCompletable()
    }

    /// Discover a new tracker with the specified name.
    ///
    /// - Parameter name: The name of the desired peripheral.
    /// - Returns: An observable that will emit the connection controller of the newly discovered tracker.
    private func connectionControllerForNewDiscoveredTracker(with name: String) -> Observable<DefaultConnectionController> {
        return self.scanner.peers
            .flatMap { return Observable.from($0) }
            .filter { $0.name.value == name }
            .map { peer in
                let descriptor = BluetoothPeerDescriptor(identifier: peer.identifier)
                return self.peerManager.getOrCreate(bluetoothPeerDescriptor: descriptor, name: peer.name.value).connectionController
            }
            .take(1)
    }
}

private extension ConnectionRemoteApi {
    enum Method: String, CaseIterable {
        case connect                    = "bt/connect"
        case getConnectionConfiguration = "bt/connection_service/get_connection_configuration"
        case getConnectionServiceStatus = "bt/connection_service/get_connection_service_status"
        case disconnect                 = "bt/disconnect"
    }
}

private extension ReconnectingConnectionController {
    func triggerConnect() {
        _ = establishConnection().subscribe()
    }
}

private struct ConnectParams: Decodable {
    let peerId: String
}

private struct GetConfigurationResult: Encodable {
    let connectionInterval: UInt16
    let slaveLatency: UInt16
    let supervisionTimeout: UInt16
    let MTU: UInt16
    let mode: String
}

private struct GetStatusResult: Encodable {
    let bondedFlag: Bool
    let encryptedFlag: Bool
    let connectionServiceState: String
}
