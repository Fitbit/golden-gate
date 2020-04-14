//
//  Hub+NetworkLink.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 2/25/19.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxCocoa
import RxSwift

enum PeripheralConnectionStatusSubstate: String {
    case discoveringService
    case discoveringCharacteristics
    case subscribing
}

private struct HubNetworkLink: NetworkLink {
    let dataSink: DataSink
    let dataSource: DataSource
    let mtu: BehaviorRelay<UInt>
}

extension HubNetworkLink: CustomStringConvertible {}

extension Hub {
    func networkLinkStatus(
        for peripheralConnectionStatus: Observable<PeripheralConnectionStatus>,
        discoveredServices: Observable<[Service]>)
    -> Observable<NetworkLinkStatus> {
        return peripheralConnectionStatus.flatMapLatest { status -> Observable<NetworkLinkStatus> in
            switch status {
            case .disconnected:
                return .just(.disconnected)
            case .connecting:
                return .just(.connecting)
            case .connected(let peripheral):
                return self.networkLinkStatus(for: peripheral, discoveredServices: discoveredServices)
            }
        }
    }

    /// Waits for Gattlink Service to be discovered,
    /// then continues to discover characteristics.
    private func networkLinkStatus(
        for peripheral: Peripheral,
        discoveredServices: Observable<[Service]>)
    -> Observable<NetworkLinkStatus> {
        return Observable.deferred { [configuration] () -> Observable<NetworkLinkStatus> in
            return discoveredServices
                .requiredService(configuration.gattlinkService.serviceUUID)
                .flatMapLatest { service -> Observable<NetworkLinkStatus> in
                    return self.networkLinkStatus(peripheral: peripheral, service: service)
                }
                .startWith(.negotiating(PeripheralConnectionStatusSubstate.discoveringService))
                .logDebug("ConnectionStatus for \(peripheral):", .bindings, .next)
        }
        // Invalidate the previous CentralManagerSource in order to unsubscribe from the previous
        // characteristic in order to avoid being subscribed simultaneously to multiple instances of
        // CBCharacteristics with same UUID which confuses CoreBluetooth (if that happens, no more
        // data is received on the CBCharacteristic).
        .scan(.disconnected) { previous, current in
            switch previous {
            case .connected(let networkLink):
                (networkLink.dataSource as? CentralManagerSource)?.invalidate()
            default: break
            }

            return current
        }
    }

    /// Discovers Gattlink characteristics,
    /// then continues to create a NetworkLink from it.
    private func networkLinkStatus(peripheral: Peripheral, service: Service) -> Observable<NetworkLinkStatus> {
        return Observable.deferred { [configuration] () -> Observable<NetworkLinkStatus> in
            let txUUID = configuration.gattlinkService.txUUID
            let rxUUID = configuration.gattlinkService.rxUUID

            return service
                .discoverCharacteristics([txUUID, rxUUID])
                .asObservable()
                .flatMap { characteristics -> Observable<NetworkLinkStatus> in
                    try self
                        .transport(
                            peripheral: peripheral,
                            txCharacteristic: characteristics.first(where: { $0.uuid == txUUID })!,
                            rxCharacteristic: characteristics.first(where: { $0.uuid == rxUUID })!
                        )
                        .map { .connected($0) }
                        .startWith(.negotiating(PeripheralConnectionStatusSubstate.subscribing))
                }
                .startWith(.negotiating(PeripheralConnectionStatusSubstate.discoveringCharacteristics))
        }
    }

    /// Creates a `NetworkLink` for the given `characteristics`,
    /// and will emit it once BLE notifications to be set up.
    ///
    /// - Parameters:
    ///   - peripheral: The Hub peripheral.
    ///   - txCharacteristic: The characteristic that transmits data from the Hub to the Node.
    ///   - rxCharacteristic: The characteristic that received data from the Node to the Hub.
    private func transport(
        peripheral: Peripheral,
        txCharacteristic: Characteristic,
        rxCharacteristic: Characteristic) throws
    -> Observable<NetworkLink> {
        return Observable.deferred { [runLoop] () -> Observable<NetworkLink> in
            // Discover the fastest communication mechanism (Write or WriteWithoutResponse)
            // and throw if we can't find any characteristics writable at all.
            guard let writeType = CBCharacteristicWriteType(fastestFor: rxCharacteristic.properties) else {
                throw NodeError.unexpectedCharacteristicProperties
            }

            let dataSink = CentralManagerSink(
                runLoop: runLoop,
                peripheral: peripheral,
                characteristic: rxCharacteristic
            )

            let dataSource = CentralManagerSource(
                runLoop: runLoop,
                peripheral: peripheral,
                characteristic: txCharacteristic
            )

            let networkLink = HubNetworkLink(
                dataSink: dataSink,
                dataSource: dataSource,
                mtu: .init(value: UInt(peripheral.maximumWriteValueLength(for: writeType)))
            )

            return .just(networkLink)
        }
    }
}

private extension CBCharacteristicWriteType {
    init?(fastestFor properties: CBCharacteristicProperties) {
        if properties.contains(.writeWithoutResponse) {
            self = .withoutResponse
        } else if properties.contains(.write) {
            self = .withResponse
        } else {
            return nil
        }
    }
}
