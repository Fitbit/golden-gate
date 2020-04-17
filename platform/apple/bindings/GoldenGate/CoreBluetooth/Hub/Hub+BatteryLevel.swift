//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Hub+BatteryLevel.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 3/1/19.
//

import Foundation
import RxBluetoothKit
import RxCocoa
import RxSwift

extension Hub {
    func remoteBatteryLevel(from characteristic: Characteristic) -> Observable<BatteryServiceLevel> {
        return Observable.merge(
                characteristic.observeValueUpdateAndSetNotification(),
                characteristic.readValue().asObservable()
            )
            .map {
                guard
                    let value = $0.value,
                    let level = BatteryServiceLevel(rawValue: value)
                    else {
                        throw HubError.illegalBatteryServiceLevel
                }

                return level
            }
    }
}
