//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BatteryServiceLevel.swift
//  GoldenGate-iOS
//
//  Created by Sylvain Rebaud on 12/8/18.
//

import Foundation

public struct BatteryServiceLevel: RawRepresentable {
    public let level: UInt8

    public init?(rawValue: Data) {
        guard rawValue.count >= 1 else {
            LogBindingsWarning("BatteryServiceLevel was \(rawValue.count) bytes - expected: 1")
            return nil
        }

        level = rawValue.asLittleEndian()
    }

    public var rawValue: Data {
        return level.toLittleEndian()
    }
}
