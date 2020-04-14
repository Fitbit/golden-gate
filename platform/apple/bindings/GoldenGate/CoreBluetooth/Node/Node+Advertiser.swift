//
//  Node+Advertiser.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/22/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxCocoa
import RxSwift

extension Node {
    public class Advertiser {
        private let peripheralManager: PeripheralManager
        private let configuration: BluetoothConfiguration
        private let didObserveSubscription = PublishSubject<BluetoothPeerDescriptor>()
        private let didObserveUnsubscription = PublishSubject<BluetoothPeerDescriptor>()
        private let disposeBag = DisposeBag()

        /// Emits peers that subscribed to Gattlink service characteristics exposed by this client.
        public var subscribedBluetoothPeerDescriptor: Observable<BluetoothPeerDescriptor> {
            return didObserveSubscription
                .asObservable()
                .logInfo("subscribed centrals", .bluetooth, .next)
        }

        /// Emits peers that unsubscribed from Gattlink service characteristics exposed by this client.
        public var unsubscribedBluetoothPeerDescriptor: Observable<BluetoothPeerDescriptor> {
            return didObserveUnsubscription
                .asObservable()
                .logInfo("unsubscribed centrals", .bluetooth, .next)
        }

        /// The name of the peripheral.
        ///
        /// - Note: Any active advertisement will be restarted when the name is changed.
        public let name: BehaviorRelay<String>

        public init(
            peripheralManager: PeripheralManager,
            gattlinkService: GattlinkService,
            configuration: BluetoothConfiguration
        ) {
            self.peripheralManager = peripheralManager
            self.configuration = configuration
            self.name = peripheralManager.name

            Observable
                .of(
                    Observable.deferred {
                        Observable.from(gattlinkService.subscribedCentrals)
                    },
                    peripheralManager.centralDidSubscribeToCharacteristic
                        .logInfo("Central did subscribe to characteristic", .bluetooth, .next)
                )
                .merge()
                .subscribe(onNext: { [configuration, didObserveSubscription] tuple -> Void in
                    if tuple.characteristic.uuid == configuration.gattlinkService.txUUID {
                        didObserveSubscription.on(.next(BluetoothPeerDescriptor( identifier: tuple.central.hackyIdentifier)))
                    }
                })
                .disposed(by: disposeBag)

            peripheralManager.centralDidUnsubscribeFromCharacteristic
                .subscribe(onNext: { [configuration, didObserveUnsubscription] tuple -> Void in
                    if tuple.characteristic.uuid == configuration.gattlinkService.txUUID {
                        didObserveUnsubscription.on(.next(BluetoothPeerDescriptor(identifier: tuple.central.hackyIdentifier)))
                    }
                })
                .disposed(by: disposeBag)
        }

        /// Whether the Advertiser is advertising at the moment.
        public var isAdvertising: Observable<Bool> {
            return peripheralManager.isAdvertising
        }

        /// Starts advertising upon subscription and stops upon disposal.
        ///
        /// - Note: If this API is used from multiple locations,
        ///   the advertising will only stop when all subscriptions have been disposed.
        public func advertise() -> Completable {
            return peripheralManager.advertise()
        }
    }
}
