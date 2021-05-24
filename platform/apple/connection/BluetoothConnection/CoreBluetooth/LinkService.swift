//
//  LinkService.swift
//  BluetoothConnection
//
//  Created by Sylvain Rebaud on 3/23/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxSwift

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable identifier_name

public class LinkService: PeripheralManagerService {
    public let advertise: Bool = true

    public weak var peripheralManager: PeripheralManager?

    public let service: CBMutableService

    public let didReceiveWrite = PublishSubject<WriteRequest>()

    public let didReceiveRead = PublishSubject<ReadRequest>()

    /// The characterstic on which data is received from other peers.
    public let rx: CBMutableCharacteristic

    /// The characteristic on which data is sent to other peers.
    public let tx: CBMutableCharacteristic

    private let disposeBag = DisposeBag()

    public init(configuration: BluetoothConfiguration.LinkServiceConfiguration, peripheralManager: PeripheralManager) {
        tx = CBMutableCharacteristic(
            type: configuration.txUUID,
            properties: [.notify],
            value: nil,
            permissions: []
        )

        rx = CBMutableCharacteristic(
            type: configuration.rxUUID,
            properties: [.writeWithoutResponse],
            value: nil,
            permissions: [.writeable]
        )

        service = CBMutableService(
            type: configuration.serviceUUID,
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

    /// Returns the subscription state to Link service for a given central identifier
    ///
    /// Returns an observable which emits a CentralSubscription every time the Link service
    /// is subscribed or unsubscribed by the given central identifier, initially starting with
    /// .unsusbscribed then complete or error.
    public func centralSubscriptionStatus(for identifier: UUID) -> Observable<CentralSubscriptionStatus> {
        guard let peripheralManager = peripheralManager else {
            LogBluetoothError("[LinkService]: Peripheral manager is deallocated.")
            return .just(.unsubscribed)
        }

        let subscribed = peripheralManager.centralDidSubscribeToCharacteristic
        let unsubscribed = peripheralManager.centralDidUnsubscribeFromCharacteristic

        return peripheralManager
            .stateOncePoweredOn()
            .flatMapLatest { [tx] state -> Observable<CentralSubscriptionStatus> in
                guard state == .poweredOn else { return Observable.just(.unsubscribed) }

                let subscribedCentrals = tx.subscribedCentrals ?? []
                LogBluetoothInfo("[LinkService]: Current subscribed centrals \(subscribedCentrals)")

                return Observable.from(subscribedCentrals)
                    .concat(
                        subscribed
                            .filter { $0.characteristic.uuid == self.tx.uuid }
                            .do(onNext: { LogBluetoothDebug("[LinkService]: Central \($0.central) subscribed to \($0.characteristic)") })
                            .map { $0.central }
                    )
                    // verify it's the Central we're interested in
                    .filter { $0.identifier == identifier }
                    .flatMap { central -> Observable<CentralSubscriptionStatus> in
                        unsubscribed
                            .filter { $0.characteristic.uuid == self.tx.uuid }
                            .filter { $0.central.identifier == identifier }
                            .do(onNext: { LogBluetoothWarning("[LinkService]: Central \($0.central) unsubscribed from \($0.characteristic)") })
                            .take(1)
                            .map { _ -> CentralSubscriptionStatus in
                                throw PeripheralManagerError.unsubscribed
                            }
                            .startWith(.subscribed(central))
                    }
            }
            .startWith(.unsubscribed)
            .do(onSubscribe: { LogBluetoothDebug("[LinkService]: Waiting for central \(identifier) to subscribe...") })
            .do(onNext: { LogBluetoothInfo("[LinkService]: Central subscription changed: \($0)") })
            .do(onError: { LogBluetoothWarning("[LinkService]: Central subscription error: \($0)") })
            .retryWhen { errors in
                errors.flatMapLatest { error -> Observable<Void> in
                    switch error {
                    case PeripheralManagerError.unsubscribed:
                        return Observable.just(()).delaySubscription(
                            .milliseconds(250),
                            scheduler: peripheralManager.scheduler
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
