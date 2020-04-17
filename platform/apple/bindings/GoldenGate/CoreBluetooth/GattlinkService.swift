//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GattlinkService.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 3/23/18.
//

import CoreBluetooth
import Foundation
import RxSwift

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable identifier_name

public class GattlinkService: PeripheralManagerService {
    public let advertise: Bool = true

    public weak var peripheralManager: PeripheralManager?

    public let service: CBMutableService

    public let didReceiveWrite = PublishSubject<WriteRequest>()

    public let didReceiveRead = PublishSubject<ReadRequest>()

    /// The characterstic on which data is received from other peers.
    let rx: CBMutableCharacteristic

    /// The characteristic on which data is sent to other peers.
    let tx: CBMutableCharacteristic

    private let disposeBag = DisposeBag()

    public init(configuration: BluetoothConfiguration, peripheralManager: PeripheralManager) {
        tx = CBMutableCharacteristic(
            type: configuration.gattlinkService.txUUID,
            properties: [.notify],
            value: nil,
            permissions: []
        )

        rx = CBMutableCharacteristic(
            type: configuration.gattlinkService.rxUUID,
            properties: [.writeWithoutResponse],
            value: nil,
            permissions: [.writeable]
        )

        service = CBMutableService(
            type: configuration.gattlinkService.serviceUUID,
            primary: true
        )

        service.characteristics = [tx, rx]

        self.peripheralManager = peripheralManager

        didReceiveRead
            .subscribe(onNext: { request in
                request.respond(withResult: .failure(.readNotPermitted))
            })
            .disposed(by: disposeBag)
    }

    /// Returns the subscription state to Gattlink service for a given central identifier
    ///
    /// Returns an observable which emits a CentralSubscription every time the Gattlink service
    /// is subscribed or unsubscribed by the given central identifier, initially starting with
    /// .unsusbscribed then complete or error.
    public func centralSubscriptionStatus(for identifier: UUID) -> Observable<CentralSubscriptionStatus> {
        guard let peripheralManager = peripheralManager else {
            LogBluetoothError("[GattlinkService]: Peripheral manager is deallocated.")
            return .just(.unsubscribed)
        }

        let subscribed = peripheralManager.centralDidSubscribeToCharacteristic
        let unsubscribed = peripheralManager.centralDidUnsubscribeFromCharacteristic

        return peripheralManager
            .stateOncePoweredOn()
            .flatMapLatest { [tx] state -> Observable<CentralSubscriptionStatus> in
                guard state == .poweredOn else { return Observable.just(.unsubscribed) }

                let subscribedCentrals = tx.subscribedCentrals ?? []
                LogBluetoothInfo("[GattlinkService]: Current subscribed centrals \(subscribedCentrals)")

                return Observable.from(subscribedCentrals)
                    .concat(
                        subscribed
                            .filter { $0.characteristic.uuid == self.tx.uuid }
                            .do(onNext: { LogBluetoothDebug("[GattlinkService]: Gattlink central \($0.central) subscribed to \($0.characteristic)") })
                            .map { $0.central }
                    )
                    // verify it's the Central we're interested in
                    .filter { $0.hackyIdentifier == identifier }
                    .flatMap { central -> Observable<CentralSubscriptionStatus> in
                        unsubscribed
                            .filter { $0.characteristic.uuid == self.tx.uuid }
                            .filter { $0.central.hackyIdentifier == identifier }
                            .do(onNext: { LogBluetoothWarning("[GattlinkService]: Gattlink central \($0.central) unsubscribed from \($0.characteristic)") })
                            .take(1)
                            .map { _ -> CentralSubscriptionStatus in
                                throw PeripheralManagerError.unsubscribed
                            }
                            .startWith(.subscribed(central))
                    }
            }
            .startWith(.unsubscribed)
            .do(onSubscribe: { LogBluetoothDebug("[GattlinkService]: Waiting for gattlink central \(identifier) to subscribe...") })
            .do(onNext: { LogBluetoothInfo("[GattlinkService]: Gattlink central subscription changed: \($0)") })
            .do(onError: { LogBluetoothWarning("[GattlinkService]: Gattlink central subscription error: \($0)") })
            .retryWhen { errors in
                errors.flatMapLatest { error -> Observable<Void> in
                    switch error {
                    case PeripheralManagerError.unsubscribed:
                        return Observable.just(()).delaySubscription(
                            .milliseconds(250),
                            scheduler: SerialDispatchQueueScheduler(qos: .default)
                        )
                    default:
                        return .error(error)
                    }
                }
            }
    }

    public var subscribedCentrals: [SubscribedCentral] {
        let characteristics = [tx]

        return characteristics.flatMap { characteristic -> [SubscribedCentral] in
            return (characteristic.subscribedCentrals ?? []).map {
                (central: $0, characteristic: characteristic as CBCharacteristic)
            }
        }
    }
}
