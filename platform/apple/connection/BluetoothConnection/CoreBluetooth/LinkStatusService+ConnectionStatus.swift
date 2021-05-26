//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  LinkStatusService+ConnectionStatus.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//

import Foundation

// swiftlint:disable nesting

extension LinkStatusService {
    public struct ConnectionStatus: CustomStringConvertible, Codable {
        struct Flags: OptionSet {
            let rawValue: UInt8

            static let bonded = Flags(rawValue: 1 << 0)
            static let encrypted = Flags(rawValue: 1 << 1)
            static let dleEnabled = Flags(rawValue: 1 << 2)
            static let dleRebootRequired = Flags(rawValue: 1 << 3)
            static let halfBonded = Flags(rawValue: 1 << 4)
        }

        /// true if bonded to any peer devices, false otherwise
        public let bonded: Bool

        /// true if bonded to current peer devices, false otherwise
        public let encrypted: Bool

        /// true if data length extensions (DLE) are on, false otherwise
        public let dleEnabled: Bool

        /// true if DLE reboot is required, false otherwise
        public let dleRebootRequired: Bool

        /// true if half-bonded state detected, false otherwise
        public let halfBonded: Bool

        /// valid if DLE flag is set, valid range: 0x1B-0xFB
        let maxTxPayloadLength: UInt8

        /// valid if DLE flag is set, valid range: 0x148-0x848
        let maxTxTime: UInt16

        /// valid if DLE flag is set, valid range: 0x1B-0xFB
        let maxRxPayloadLength: UInt8

        /// valid if DLE flag is set, valid range: 0x148-0x848
        let maxRxTime: UInt16

        public var description: String {
            let values: [String?] = [
                bonded ? "bonded" : nil,
                encrypted ? "encrypted" : nil,
                dleEnabled ? "DLE enabled" : nil,
                dleRebootRequired ? "DLE reboot required" : nil,
                halfBonded ? "half bondend" : nil,
                "maxTxPayloadLength: \(maxTxPayloadLength)",
                "maxTxTime: \(maxTxTime)",
                "maxRxPayloadLength: \(maxRxPayloadLength)",
                "maxRxTime: \(maxRxTime)"
            ]

            return values
                .compactMap { $0 }
                .joined(separator: "|")
        }
    }
}

extension LinkStatusService.ConnectionStatus: RawRepresentable {
    public init?(rawValue: Data) {
        guard rawValue.count >= 7 else {
            LogBluetoothWarning("ConnectionStatus was \(rawValue.count) bytes - expected: 7")
            return nil
        }

        let bytes = [UInt8](rawValue)

        do {
            let flags = Flags(rawValue: bytes[0])
            bonded = flags.contains(.bonded)
            encrypted = flags.contains(.encrypted)
            dleEnabled = flags.contains(.dleEnabled)
            dleRebootRequired = flags.contains(.dleRebootRequired)
            halfBonded = flags.contains(.halfBonded)
        }

        maxTxPayloadLength = rawValue[1...1].asLittleEndian()
        maxTxTime = rawValue[2...3].asLittleEndian()
        maxRxPayloadLength = rawValue[4...4].asLittleEndian()
        maxRxTime = rawValue[5...6].asLittleEndian()
    }

    public var rawValue: Data {
        var flags: Flags = []
        flags.set(.bonded, bonded)
        flags.set(.encrypted, encrypted)
        flags.set(.dleEnabled, dleEnabled)
        flags.set(.dleRebootRequired, dleRebootRequired)
        flags.set(.halfBonded, halfBonded)

        var result = Data()

        result.append(littleEndian: flags.rawValue)
        result.append(littleEndian: maxTxPayloadLength)
        result.append(littleEndian: maxTxTime)
        result.append(littleEndian: maxRxPayloadLength)
        result.append(littleEndian: maxRxTime)

        return result
    }
}

private extension OptionSet {
    mutating func set(_ flag: Element, _ present: Bool) {
        if present {
            insert(flag)
        } else {
            remove(flag)
        }
    }
}
