//
//  Hub+BatteryLevel.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 3/1/19.
//  Copyright © 2019 Fitbit. All rights reserved.
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
