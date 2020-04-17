//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CharacteristicUpdater.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 27/02/2019.
//

import CoreBluetooth
import RxSwift

/// Object that updates a characteristic value on specified centrals. If the update fails,
/// the updater will retry it when `readyToUpdateSubscribers` emits a new value.
/// Any pending update on given subscribed centrals is cancelled by a subsequent update
/// on the same subscribed centrals.
final class CharacteristicUpdater {
    private let writer: RetryingWriter<CharacteristicSubscription>

    init(peripheralManager: CBPeripheralManager, readyToUpdateSubscribers: Observable<Void>, scheduler: SchedulerType) {
        self.writer = RetryingWriter(
            write: { data, group in
                peripheralManager.updateValue(
                    data,
                    for: group.characteristic,
                    onSubscribedCentrals: group.subscriber.map { [$0] }
                )
            },
            retryTrigger: readyToUpdateSubscribers,
            scheduler: scheduler
        )
    }

    /// Update a characteristic value on specified centrals. If the update fails, it will be retried
    /// when `readyToUpdateSubscribers` emits a new value. The update will cancel any pending updates
    /// on the same subscribed centrals.
    ///
    /// - Parameters:
    ///   - value: The value to be sent to specified subscribed centrals.
    ///   - characteristic: The characteristic whose value has changed.
    ///   - central:  A `CBCentral` object to receive the update. If `nil`, all centrals
    ///         that are subscribed to `characteristic` will be updated.
    func updateValue(_ value: Data, for characteristic: CBMutableCharacteristic, onSubscribedCentral central: CBCentral?) {
        let subscription = CharacteristicSubscription(subscriber: central, characteristic: characteristic)
        writer.writeRelay.accept((data: value, target: subscription))
    }
}

private extension CharacteristicUpdater {
    struct CharacteristicSubscription: Hashable {
        let subscriber: CBCentral?
        let characteristic: CBMutableCharacteristic
    }
}
