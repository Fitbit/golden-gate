//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothAdvertiser.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 11/22/17.
//

import CoreBluetooth
import Foundation
import RxRelay
import RxSwift

public class BluetoothAdvertiser {
    private let peripheralManager: PeripheralManager
    private let didObserveSubscription = PublishSubject<PeerDescriptor>()
    private let didObserveUnsubscription = PublishSubject<PeerDescriptor>()
    private let disposeBag = DisposeBag()

    /// Emits peers that subscribed to Link service characteristics exposed by this client.
    public var subscribedPeerDescriptor: Observable<PeerDescriptor> {
        return didObserveSubscription
            .asObservable()
            .logInfo("subscribed centrals", .bluetooth, .next)
    }

    /// Emits peers that unsubscribed from Link service characteristics exposed by this client.
    public var unsubscribedPeerDescriptor: Observable<PeerDescriptor> {
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
        linkService: LinkService
    ) {
        self.peripheralManager = peripheralManager
        self.name = peripheralManager.name

        let txUUID = linkService.tx.uuid

        Observable
            .of(
                Observable.deferred {
                    Observable.from(linkService.subscribedCentrals)
                },
                peripheralManager.centralDidSubscribeToCharacteristic
                    .logInfo("Central did subscribe to characteristic", .bluetooth, .next)
            )
            .merge()
            .subscribe(onNext: { [didObserveSubscription] central, characteristic -> Void in
                if characteristic.uuid == txUUID {
                    didObserveSubscription.on(.next(PeerDescriptor( identifier: central.identifier)))
                }
            })
            .disposed(by: disposeBag)

        peripheralManager.centralDidUnsubscribeFromCharacteristic
            .subscribe(onNext: { [didObserveUnsubscription] central, characteristic -> Void in
                if characteristic.uuid == txUUID {
                    didObserveUnsubscription.on(.next(PeerDescriptor(identifier: central.identifier)))
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
