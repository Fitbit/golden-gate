//
//  PeerDiscoverer.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 18.06.2021.
//  Copyright © 2021 Fitbit. All rights reserved.
//

import Foundation
import CoreBluetooth
import RxBluetoothKit
import RxSwift

/// A retriever of connected and scanner for nearby Bluetooth peers. The discoverer supports matching by peer and service identifier and it guarantees
/// that each peer is emitted only once for the entire lifetime of the subscription. Various attributes of the peers are updated as the discovery process continues.
public protocol PeerDiscovererType {
    /// Observable that emits the stabilized Bluetooth state.
    var bluetoothState: Observable<BluetoothState> { get }

    /// An observable that starts polling for connected peripherals upon subscription and emits distinct peers as they are discovered. The polling is stopped
    /// upon disposal.
    ///
    /// It matches peers that implement any of the services specified and pass the peer filter provided. Each discovered peer is connected to and its services are
    /// discovered. Each discovered peer's rssi is updated periodically.
    ///
    /// The observable never completes and never errors out if Bluetooth is powered on. If connecting to a peer or discovering its services fail, the peer is discarded.
    /// If rssi reading fails for a peer, the respective reading is ignored, but the rssi updates continue.
    ///
    /// If Bluetooth is not powered on, the observable errors out.
    ///
    /// - Parameters:
    ///   - serviceUuids: A list of services. A peer must implement any service from this list to be matched.
    ///   - peerFilter: A filter for discovered peers based on peer's identifier.
    func connectedPeers(withServices serviceUuids: [CBUUID], peerFilter: ((UUID) -> Bool)?) -> Observable<DiscoveredPeerType>

    /// An observable that starts scanning for nearby peripherals upon subscription and emits distinct peers as they are discovered. The scanning is stopped
    /// upon disposal.
    ///
    /// It matches peers that implement any of the services specified and pass the peer filter provided. Each discovered peer's rssi is updated from time to time.
    ///
    /// The observable never completes and never errors out if Bluetooth is powered on.
    ///
    /// If Bluetooth is not powered on, the observable errors out.
    ///
    /// - Parameters:
    ///   - serviceUuids: A  list of services. A peer must implement any service from this list to be matched. If the list is nil, any peer is matched. Note that
    ///   due to limitations in CoreBluetooth, scanning with a `nil` list of services does not work in background.
    ///   - peerFilter: A filter for discovered peers based on peer's identifier.
    func scannedPeers(withServices serviceUuids: [CBUUID]?, peerFilter: ((UUID) -> Bool)?) -> Observable<DiscoveredPeerType>

    /// An observable that starts both polling for connected peripherals and scanning for nearby peripherals upon subscription and emits distinct peers as they are discovered.
    /// The polling and scanning are stopped upon disposal.
    ///
    /// - SeeAlso: `connectedPeers` and `scannedPeers`.
    func discoveredPeers(scannedServices: [CBUUID]?, connectedServices: [CBUUID], peerFilter: ((UUID) -> Bool)?) -> Observable<DiscoveredPeerType>
}

public final class PeerDiscoverer: PeerDiscovererType {
    private static let connectedPeripheralsQueryInterval = DispatchTimeInterval.seconds(2)
    private static let rssiUpdateInterval = DispatchTimeInterval.seconds(1)

    private let centralManager: CentralManagerType
    private let bluetoothScheduler: SchedulerType

    public var bluetoothState: Observable<BluetoothState> {
        centralManager.stabilizedState()
    }

    /// Observable that emits if Bluetooth is powered on and errors out otherwise.
    private var ensureBluetoothOn: Observable<Void> {
        bluetoothState
            .map { state -> Void in
                guard state == .poweredOn else {
                    throw BluetoothError(state: state) ?? .bluetoothInUnknownState
                }
                return ()
            }
    }

    public init(centralManager: CentralManagerType, bluetoothScheduler: SchedulerType) {
        self.centralManager = centralManager
        self.bluetoothScheduler = bluetoothScheduler
    }

    public func connectedPeers(withServices serviceUuids: [CBUUID], peerFilter: ((UUID) -> Bool)?) -> Observable<DiscoveredPeerType> {
        // The peer provider enforces uniqueness among the connected peers
        connectedPeers(withServices: serviceUuids, peerFilter: peerFilter, peerProvider: Self.makeUniquePeerProvider())
    }

    private func connectedPeers(
        withServices serviceUuids: [CBUUID],
        peerFilter: ((UUID) -> Bool)?,
        peerProvider: @escaping (PeripheralType) -> PeerProviderResult
    ) -> Observable<DiscoveredPeerType> {
        let peripherals: Observable<PeripheralType> = Observable<Int>
            .interval(Self.connectedPeripheralsQueryInterval, scheduler: self.bluetoothScheduler)
            .startWith(0)
            .flatMapFirst { _ -> Observable<PeripheralType> in
                let connectedPeripherals: [PeripheralType] = self.centralManager.retrieveConnectedPeripherals(withServices: serviceUuids)
                    .filter { peerFilter?($0.identifier) ?? true }
                    // Remove duplicate peripherals retrieved from the system
                    .reduce([]) { uniquePeripherals, peripheral in
                        if uniquePeripherals.contains(where: { $0.identifier == peripheral.identifier }) {
                            return uniquePeripherals
                        } else {
                            return uniquePeripherals + [peripheral]
                        }
                    }

                return Observable.from(connectedPeripherals)
            }

        let peers: Observable<DiscoveredPeerType> = peripherals
            .groupBy { $0.identifier } // Group to be able to limit to 1 the number of simultaneous connections to each peer
            .flatMap { samePeripheral -> Observable<DiscoveredPeerType> in
                // Don't connect if there's another active connection to the same peripheral
                samePeripheral.flatMapFirst { peripheral -> Observable<DiscoveredPeerType> in
                    Observable.just(peripheral)
                        .flatMap { peripheral -> Observable<PeripheralType> in
                            // The peripheral might be connected at system level, a connection being needed to read RSSI information
                            peripheral.establishConnection(options: nil)
                                .logInfo("PeerDiscoverer: Connecting to already connected peripheral \(peripheral)", .bluetooth, .subscribe)
                                .logError("PeerDiscoverer: Failed to establish connection to connected peripheral \(peripheral) with error", .bluetooth, .error)
                        }
                        .flatMap { peripheral -> Observable<PeripheralType> in
                            // Discover services to allow the returned peer to be queried for service identifiers
                            peripheral.discoverServices(serviceUuids)
                                .logDebug("PeerDiscoverer: Discovering services for connected peripheral", .bluetooth, .subscribe)
                                .logError("PeerDiscoverer: Failed to discover services for connected peripheral \(peripheral) with error", .bluetooth, .error)
                                .asObservable()
                                .map { _ in peripheral }
                        }
                        .flatMap { peripheral -> Observable<DiscoveredPeerType> in
                            let peerResult = peerProvider(peripheral)
                            let peer: DiscoveredPeer
                            let newPeer: Bool

                            switch peerResult {
                            case let .new(providedPeer):
                                peer = providedPeer
                                newPeer = true
                            case let .existing(providedPeer):
                                peer = providedPeer
                                newPeer = false
                            }

                            // Interrogate the RSSI of the connected peripheral every second.
                            let rssiUpdate = Observable<Int>
                                .interval(Self.rssiUpdateInterval, scheduler: self.bluetoothScheduler)
                                .startWith(0)
                                .flatMapFirst { _ in
                                    peripheral.readRSSI()
                                        .logDebug("PeerDiscoverer: Updated RSSI for connected peripheral", .bluetooth, .next)
                                        .do(onNext: {
                                            peer.update(peripheral: peripheral, advertisementData: nil, rssi: $0.1)
                                        })
                                        .ignoreElements()
                                        .asCompletable()
                                        .catch { _ in Completable.empty() }
                                }

                            // Emit the peer only if it has never been discovered previously
                            return (newPeer ? Observable.just(peer) : Observable.empty())
                                // Then update the rssi without emitting anything
                                .concat(rssiUpdate.map { _ in peer as DiscoveredPeerType })
                        }
                        // End the observable when there's an error
                        .catch { _ in Observable.empty() }
                }
            }

        return ensureBluetoothOn
            .logError("PeerDiscoverer: Encountered bluetooth error while retrieving connected peers", .bluetooth, .error)
            .flatMapLatest { _ in
                peers.logError("PeerDiscoverer: Encountered error while retrieving connected peers", .bluetooth, .error)
            }
    }

    public func scannedPeers(withServices serviceUuids: [CBUUID]?, peerFilter: ((UUID) -> Bool)?) -> Observable<DiscoveredPeerType> {
        // The peer provider enforces uniqueness among the scanned peers
        scannedPeers(withServices: serviceUuids, peerFilter: peerFilter, peerProvider: Self.makeUniquePeerProvider())
    }

    private func scannedPeers(
        withServices serviceUuids: [CBUUID]?,
        peerFilter: ((UUID) -> Bool)?,
        peerProvider: @escaping (PeripheralType) -> PeerProviderResult
    ) -> Observable<DiscoveredPeerType> {
        let peers: Observable<DiscoveredPeerType> = centralManager
            .scanForPeripherals(
                withServices: serviceUuids,
                options: [
                    // Easy way to get repeated RSSI readings
                    CBCentralManagerScanOptionAllowDuplicatesKey: true as NSNumber
                ]
            )
            .filter { peerFilter?($0.peripheral().identifier) ?? true }
            .groupBy { $0.peripheral().identifier } // Group to be able to throttle rssi updates per peer
            .flatMap { samePeripheral -> Observable<ScannedPeripheralType> in
                samePeripheral.throttle(Self.rssiUpdateInterval, scheduler: self.bluetoothScheduler)
            }
            .compactMap { scannedPeripheral -> DiscoveredPeerType? in
                let peerResult = peerProvider(scannedPeripheral.peripheral())

                switch peerResult {
                case let .new(peer):
                    peer.update(peripheral: scannedPeripheral.peripheral(), advertisementData: scannedPeripheral.advertisementData, rssi: scannedPeripheral.rssi.intValue)
                    LogInfo("PeerDiscoverer: Scanned peripheral \(scannedPeripheral)", domain: .bluetooth)
                    return peer
                case let .existing(peer):
                    peer.update(peripheral: scannedPeripheral.peripheral(), advertisementData: scannedPeripheral.advertisementData, rssi: scannedPeripheral.rssi.intValue)
                    LogDebug("PeerDiscoverer: Updated RSSI for scanned peripheral \(scannedPeripheral)", domain: .bluetooth)
                    return nil
                }
            }

        return ensureBluetoothOn
            .logError("PeerDiscoverer: Encountered bluetooth error while scanning for peers", .bluetooth, .error)
            .flatMapLatest { _ in
                peers.logError("PeerDiscoverer: Encountered error while scanning for peers", .bluetooth, .error)
            }
    }

    public func discoveredPeers(scannedServices: [CBUUID]?, connectedServices: [CBUUID], peerFilter: ((UUID) -> Bool)?) -> Observable<DiscoveredPeerType> {
        Observable.deferred {
            // An unique peer provider shared among scanned and connected peers. Peer uniqueness is guaranteed when retrieving
            // connected and scanned peers separately, but the provider enforces peer uniqueness in these rare cases that occur
            // when dealing with both types of peers:
            // - Peers that have previously been scanned, but got connected in the meantime
            // - Peers that have previously been connected, but started advertising in the meantime
            let peerProvider = Self.makeUniquePeerProvider()

            return Observable<DiscoveredPeerType>
                .merge(
                    self.connectedPeers(withServices: connectedServices, peerFilter: peerFilter, peerProvider: peerProvider),
                    self.scannedPeers(withServices: scannedServices, peerFilter: peerFilter, peerProvider: peerProvider)
                )
        }
    }
}

private extension PeerDiscoverer {
    /// The result returned by a peer provider
    enum PeerProviderResult {
        /// Describes the case where the provider creates a new peer instance
        case new(DiscoveredPeer)
        /// Describes the case where the provider retrieves an existing peer instance
        case existing(DiscoveredPeer)
    }

    /// Creates a provider that assures the uniqueness of the peers it returns, i.e. for a given identifier, the same peer instance is returned.
    static func makeUniquePeerProvider() -> (PeripheralType) -> PeerProviderResult {
        var cache: [UUID: DiscoveredPeer] = [:]

        return { peripheral -> PeerProviderResult in
            if let knownPeer = cache[peripheral.identifier] {
                return .existing(knownPeer)
            } else {
                let peer = DiscoveredPeer(peripheral: peripheral)
                cache[peripheral.identifier] = peer
                return .new(peer)
            }
        }
    }
}

extension PeerDiscovererType {
    /// Emits a list of discovered peers that implement any of the services specified.
    ///
    /// - Note: Will only discover peripherals while a subscription exists.
    ///
    /// - Returns: Observable
    ///   - Value: Whenever the list of discovered peers changes. Nil if currently not discovering due to Bluetooth being unavailable.
    ///   - Error: Never
    ///   - Complete: Never
    public func peerList(withServices serviceUuids: [CBUUID], peerFilter: ((UUID) -> Bool)?) -> Observable<[DiscoveredPeerType]?> {
        bluetoothState
            .flatMapLatest { state -> Observable<[DiscoveredPeerType]?> in
                guard state == .poweredOn else { return .just(nil) }

                return self.discoveredPeers(scannedServices: serviceUuids, connectedServices: serviceUuids, peerFilter: peerFilter)
                    .scan([]) { peers, peer -> [DiscoveredPeerType] in
                        peers + [peer]
                    }
                    .startWith([])
                    .optionalize()
                    .catchAndReturn(nil)
                    .logInfo("PeerDiscoverer: started discovery", .bluetooth, .subscribe)
                    .logInfo("PeerDiscoverer: stopped discovery", .bluetooth, .disposed)
            }
            .distinctUntilChanged(nilComparer)
    }
}

extension ScannedPeripheral: CustomStringConvertible {
    public var description: String {
        "ScannedPeripheral (name=\(peripheral.name ??? "-"), rssi=\(rssi), id=\(peripheral.identifier))"
    }
}
