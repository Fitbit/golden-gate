//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Hub+Scanner.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/6/17.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxCocoa
import RxSwift

extension Hub {
    /// Utility to discover GoldenGate peers.
    public class Scanner {
        private let centralManager: RxBluetoothKit.CentralManager
        private let services: [CBUUID]
        private let disposeBag = DisposeBag()

        /// The status of the scanning process.
        public let isScanning = BehaviorRelay(value: false)

        public init(centralManager: RxBluetoothKit.CentralManager, services: [CBUUID]) {
            self.centralManager = centralManager
            self.services = services

            isScanning.asObservable()
                .logInfo("Scanning:", .bluetooth, .next)
                .subscribe()
                .disposed(by: disposeBag)
        }

        public convenience init(centralManager: RxBluetoothKit.CentralManager, configuration: BluetoothConfiguration = .default) {
            self.init(centralManager: centralManager, services: [
                configuration.gattlinkService.serviceUUID
            ])
        }

        /// Emits list of discovered potential peers.
        ///
        /// - Note: Will only scan for peripherals
        ///   while a subscription to this exists.
        ///
        /// - Returns: Observable
        ///   - Value: Whenever the list of known peers changes.
        ///   - Error: Never
        ///   - Complete: Never
        public lazy var peers: Observable<[DiscoveredBluetoothPeer]> = {
            var peers = [DiscoveredBluetoothPeer]()
            var peerMap = [UUID: DiscoveredBluetoothPeer]()

            let connectedPeers = Observable.from(
                    centralManager.retrieveConnectedPeripherals(withServices: services)
                )
                .flatMap { $0.establishConnection().catchError { _ in .empty() } }
                .flatMap { [services] connectedPeripheral in
                    return connectedPeripheral.discoverServices(services)
                        .map { _ in connectedPeripheral }
                        .asObservable()
                        .catchError { _ in .empty() }
                }
                .flatMap { connectedPeripheral -> Observable<[DiscoveredBluetoothPeer]> in
                    let identifier = connectedPeripheral.identifier
                    let peer = DiscoveredBluetoothPeer(peripheral: connectedPeripheral, advertisementData: nil, rssi: nil)
                    peers.append(peer)
                    peerMap[identifier] = peer

                    // Interogate the rssi of the connected peripherals every second.
                    return Observable<Int>
                        .interval(.seconds(1), scheduler: SerialDispatchQueueScheduler(qos: .default))
                        .flatMapFirst { _ in
                            connectedPeripheral.readRSSI()
                        }
                        .do(onNext: {
                            peer.update(newRssi: NSNumber(value: $0.1))
                        })
                        .map { _ in peers }
                        .catchErrorJustReturn(peers)
                }

            let scannedPeers = centralManager.scanForPeripherals(
                    withServices: services,
                    options: [
                        // Easy way to get repeated RSSI readings
                        CBCentralManagerScanOptionAllowDuplicatesKey: true as NSNumber
                    ]
                )
                .map { scannedPeripheral -> [DiscoveredBluetoothPeer] in
                    let identifier = scannedPeripheral.peripheral.identifier

                    if let peer = peerMap[identifier] {
                        peer.merge(from: scannedPeripheral)
                    } else {
                        let peer = DiscoveredBluetoothPeer(scannedPeripheral)
                        peers.append(peer)
                        peerMap[identifier] = peer
                    }

                    return peers
                }

            return centralManager
                .observeState()
                .distinctUntilChanged()
                .flatMapLatest { [isScanning] state -> Observable<[DiscoveredBluetoothPeer]> in
                    guard state == .poweredOn else { return .just([]) }

                    return Observable.merge(
                            connectedPeers,
                            scannedPeers
                        )
                        .do(
                            onSubscribe: { isScanning.accept(true) },
                            onDispose: {
                                isScanning.accept(false)
                                peerMap.removeAll()
                                peers.removeAll()
                            }
                        )
                }
                .startWith([])
                .share(replay: 1)
        }()
    }
}
