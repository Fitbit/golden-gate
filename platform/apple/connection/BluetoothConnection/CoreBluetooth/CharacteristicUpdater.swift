//
//  CharacteristicUpdater.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 27/02/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import CoreBluetooth
import RxSwift

/// Object that updates a characteristic value on specified centrals. If the update fails,
/// the updater will retry it when `readyToUpdateSubscribers` emits a new value.
/// Any pending update on given subscribed centrals is cancelled by a subsequent update
/// on the same subscribed centrals.
final class CharacteristicUpdater {
    private let writer: RetryingWriter<CharacteristicSubscription>

    init(
        identifier: String,
        peripheralManager: CBPeripheralManager,
        readyToUpdateSubscribers: Observable<Void>,
        bluetoothScheduler: SchedulerType
    ) {
        self.writer = RetryingWriter(
            write: { data, group in
                let recipients: [CBCentral]?

                if let centralId = group.subscriberId {
                    let subscribedCentrals = group.characteristic.subscribedCentrals

                    // Look for the recipient central. Failure to retrieve the specified central
                    // will cause the update to be dropped without being retried.
                    if let central = subscribedCentrals?.first(where: { $0.identifier == centralId }) {
                        recipients = [central]
                    } else {
                        LogBluetoothError(
                            """
                            [CharacteristicUpdater(\(identifier)] Couldn't update characteristic \(group.characteristic.uuid) \
                            because no subscribed central with id \(centralId) could be found.
                            Subscribed centrals are: \(subscribedCentrals ??? "[]"))
                            """
                        )
                        // There's no point in retrying an update when its recipient doesn't exist.
                        return .failure(.unretriable)
                    }
                } else {
                    // According to CoreBluetooth docs, all centrals will be updated when specifying nil.
                    recipients = nil
                }

                let success = peripheralManager.updateValue(
                    data,
                    for: group.characteristic,
                    onSubscribedCentrals: recipients
                )

                return success ? .success(()) : .failure(.retriable)
            },
            retryTrigger: readyToUpdateSubscribers,
            scheduler: bluetoothScheduler
        )
    }

    /// Update a characteristic value on specified centrals. If no centrals match the supplied identifier,
    /// the update will fail and it will not be retried. Otherwise, if the update fails, it will be retried
    /// when `readyToUpdateSubscribers` emits a new value. The update will cancel any pending updates
    /// on the same subscribed centrals.
    ///
    /// - Parameters:
    ///   - value: The value to be sent to specified subscribed centrals.
    ///   - characteristic: The characteristic whose value has changed.
    ///   - centralUuid: The UUID of a `CBCentral` object to receive the update. If `nil`, all centrals
    ///         that are subscribed to `characteristic` will be updated.
    func updateValue(_ value: Data, forCharacteristic characteristic: CBMutableCharacteristic, onSubscribedCentralUuid centralUuid: UUID?) {
        let subscription = CharacteristicSubscription(subscriberId: centralUuid, characteristic: characteristic)
        writer.writeRelay.accept((data: value, target: subscription))
    }
}

private extension CharacteristicUpdater {
    struct CharacteristicSubscription: Hashable {
        let subscriberId: UUID?
        let characteristic: CBMutableCharacteristic
    }
}
