//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  LinkStatusService+ConnectionConfiguration.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//

import Foundation

// swiftlint:disable nesting

extension LinkStatusService {
    public struct ConnectionConfiguration: CustomStringConvertible, Codable {
        public enum Mode: UInt8, Codable, CustomStringConvertible {
            case `default` = 0
            case fast = 1
            case slow = 2

            public var description: String {
                switch self {
                case .default: return "Default"
                case .fast: return "Fast"
                case .slow: return "Slow"
                }
            }
        }

        /// in units of 1.25ms
        public var connectionInterval: UInt16

        /// in units of connection intervals
        public var slaveLatency: UInt16

        /// in units of 10 ms
        public var supervisionTimeout: UInt16

        /// in units of bytes
        public var MTU: UInt16

        public var mode: Mode

        public var description: String {
            let values: [String?] = [
                "connectionInterval: \(connectionInterval)",
                "slaveLatency: \(slaveLatency)",
                "supervisionTimeout: \(supervisionTimeout)",
                "MTU: \(MTU)",
                "Mode: \(mode)"
            ]

            return values
                .compactMap { $0 }
                .joined(separator: "|")
        }
    }
}

extension LinkStatusService.ConnectionConfiguration: RawRepresentable {
    public init?(rawValue: Data) {
        guard rawValue.count >= 9 else {
            LogBluetoothWarning("ConnectionConfiguration was \(rawValue.count) bytes - expected: 9")
            return nil
        }

        connectionInterval = rawValue[0...1].asLittleEndian()
        slaveLatency = rawValue[2...3].asLittleEndian()
        supervisionTimeout = rawValue[4...5].asLittleEndian()
        MTU = rawValue[6...7].asLittleEndian()

        let rawMode = rawValue[8]
        if let mode = Mode(rawValue: rawMode) {
            self.mode = mode
        } else {
            LogBluetoothWarning("Unknown ConnectionConfiguration.Mode: \(rawMode) (\(String(rawMode, radix: 2)))")
            self.mode = .fast
        }
    }

    public var rawValue: Data {
        var result = Data()

        result.append(littleEndian: connectionInterval)
        result.append(littleEndian: slaveLatency)
        result.append(littleEndian: supervisionTimeout)
        result.append(littleEndian: MTU)
        result.append(littleEndian: mode.rawValue)

        return result
    }
}
