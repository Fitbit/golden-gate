//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothScanner.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 11/6/17.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxRelay
import RxSwift

public final class BluetoothScanner {
    private let centralManager: CentralManagerType
    private let services: [CBUUID]
    private let bluetoothScheduler: SchedulerType
    private let disposeBag = DisposeBag()

    /// The status of the scanning process.
    public let isScanning = BehaviorRelay(value: false)

    public init(
        centralManager: CentralManagerType,
        services: [CBUUID],
        bluetoothScheduler: SchedulerType
    ) {
        self.centralManager = centralManager
        self.services = services
        self.bluetoothScheduler = bluetoothScheduler

        isScanning.asObservable()
            .logInfo("Scanner: is scanning", .bluetooth, .next)
            .subscribe()
            .disposed(by: disposeBag)
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
    public var peers: Observable<[DiscoveredBluetoothPeer]> {
        var peers = [DiscoveredBluetoothPeer]()
        var peerMap = [UUID: DiscoveredBluetoothPeer]()

        let connectedPeers = Observable.from(
                centralManager.retrieveConnectedPeripherals(withServices: services)
            )
            .logInfo("Scanner: connected peripheral", .bluetooth, .next)
            .flatMap { $0.establishConnection(options: nil).catchError { _ in .empty() } }
            .flatMap { [services] connectedPeripheral in
                return connectedPeripheral.discoverServices(services)
                    .map { _ in connectedPeripheral }
                    .asObservable()
                    .catchError { _ in .empty() }
            }
            .flatMap { [bluetoothScheduler] connectedPeripheral -> Observable<[DiscoveredBluetoothPeer]> in
                let identifier = connectedPeripheral.identifier
                let peer = DiscoveredBluetoothPeer(peripheral: connectedPeripheral, advertisementData: nil, rssi: nil)
                peers.append(peer)
                peerMap[identifier] = peer

                // Interogate the rssi of the connected peripherals every second.
                return Observable<Int>
                    .interval(.seconds(1), scheduler: bluetoothScheduler)
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
            .logInfo("Scanner: scanned peripheral", .bluetooth, .next)
            .map { scannedPeripheral -> [DiscoveredBluetoothPeer] in
                let identifier = scannedPeripheral.peripheral().identifier

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
            .observeStateWithInitialValue()
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
    }
}

extension ScannedPeripheral: CustomStringConvertible {
    public var description: String {
        return "ScannedPeripheral (name=\(peripheral.name ??? "-"), rssi=\(rssi), id=\(peripheral.identifier))"
    }
}
