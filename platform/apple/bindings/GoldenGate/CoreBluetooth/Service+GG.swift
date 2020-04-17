//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Service+GG.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/18/17.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

extension Service {
    public func discoverPartialCharacteristics(_ characteristicUUIDs: [CBUUID], timeout: RxTimeInterval = .seconds(10), timeoutScheduler: SchedulerType = SerialDispatchQueueScheduler(qos: .default)) -> Observable<CharacteristicCollection> {
        return discoverCharacteristics(characteristicUUIDs)
            .asObservable()
            .do(onSubscribe: { LogBluetoothInfo("Discovering characteristics \(characteristicUUIDs) for service \(self.uuid)...") })
            .do(onNext: { LogBluetoothInfo("Discovered characteristics \($0) for service \(self.uuid)") })
            .ignoreElements()
            /// Workaround for https://github.com/Polidea/RxBluetoothKit/issues/182
            /// which will not complete the observable if not all characteristics are found.
            .timeout(timeout, other: .empty(), scheduler: timeoutScheduler)
            .andThen(Observable.deferred { .just(CharacteristicCollection(characteristics: self.characteristics ?? [])) })
    }
}

public enum CharacteristicCollectionError: Error {
    case missingCharacteristic(CBUUID)
}

public struct CharacteristicCollection {
    var characteristics: [Characteristic]

    public func optional(_ uuid: CBUUID) -> Characteristic? {
        return characteristics.first { $0.uuid == uuid }
    }

    public func required(_ uuid: CBUUID) throws -> Characteristic {
        guard let result = optional(uuid) else {
            throw CharacteristicCollectionError.missingCharacteristic(uuid)
        }

        return result
    }
}

extension CharacteristicCollection {
    init() {
        self.init(characteristics: [])
    }
}

extension Characteristic: CustomStringConvertible {
    public var description: String {
        return "\(self.characteristic)"
    }
}
