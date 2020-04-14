//
//  BatteryServiceLevel.swift
//  GoldenGate-iOS
//
//  Created by Sylvain Rebaud on 12/8/18.
//  Copyright © 2018 Fitbit. All rights reserved.
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
