//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Hub+LinkStatusService.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 3/1/19.
//

import BluetoothConnection
import Foundation
import RxBluetoothKit
import RxCocoa
import RxSwift

extension Hub {
    func remoteConnectionConfiguration(from characteristic: CharacteristicType) -> Observable<LinkStatusService.ConnectionConfiguration> {
        return characteristic.readAndObserveValue()
            .map {
                guard
                    let value = $0,
                    let configuration = LinkStatusService.ConnectionConfiguration(rawValue: value)
                else {
                    throw HubError.illegalConnectionConfiguration
                }

                return configuration
            }
    }

    func remoteConnectionStatus(from characteristic: CharacteristicType) -> Observable<LinkStatusService.ConnectionStatus> {
        return characteristic.readAndObserveValue()
            .map {
                guard
                    let value = $0,
                    let status = LinkStatusService.ConnectionStatus(rawValue: value)
                else {
                    throw HubError.illegalConnectionStatus
                }

                return status
            }
    }
}
