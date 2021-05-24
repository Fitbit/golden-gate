//
//  LinkConfigurationService+PreferredConnectionMode.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation

// swiftlint:disable nesting

extension LinkConfigurationService {
    public struct PreferredConnectionMode: CustomStringConvertible, Codable {
        public enum Mode: UInt8, Codable, CustomStringConvertible {
            case fast = 0
            case slow = 1

            public var description: String {
                switch self {
                case .fast:
                    return "fast"
                case .slow:
                    return "slow"
                }
            }
        }

        public static let fast = PreferredConnectionMode(mode: .fast)
        public static let slow = PreferredConnectionMode(mode: .slow)
        public static let `default` = slow

        public var mode: Mode

        public var description: String {
            return "\(mode)"
        }
    }
}

extension LinkConfigurationService.PreferredConnectionMode: RawRepresentable {
    public init?(rawValue: Data) {
        guard rawValue.count >= 1 else {
            LogBluetoothWarning("PreferredConnectionMode was \(rawValue.count) bytes - expected: 1")
            return nil
        }

        let bytes = [UInt8](rawValue)

        do {
            let rawMode = bytes[0]
            if let mode = Mode(rawValue: rawMode) {
                self.mode = mode
            } else {
                LogBluetoothWarning("Unknown PreferredConnectionMode: \(rawMode) (\(String(rawMode, radix: 2)))")
                self.mode = .fast
            }
        }
    }

    public var rawValue: Data {
        return Data([mode.rawValue])
    }
}
