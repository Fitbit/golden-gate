//
//  LinkConfigurationService+PreferredConnectionConfiguration.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation

// swiftlint:disable nesting only_todo

struct ValidRange<T, U> {
    let min: T
    let max: T
    let unit: U
}

extension LinkConfigurationService {
    public struct PreferredConnectionConfiguration: CustomStringConvertible, Codable {
        struct Mask: OptionSet {
            let rawValue: UInt8

            static let fastMode = Mask(rawValue: 1 << 0)
            static let slowMode = Mask(rawValue: 1 << 1)
            static let dle = Mask(rawValue: 1 << 2)
            static let mtu = Mask(rawValue: 1 << 3)
        }

        public struct ModeConfiguration: CustomStringConvertible, Codable {
            /// in units of 1.25ms, valid range: 15 msec to 2 seconds
            var min: UInt16

            /// in units of 1.25ms, valid range: 15 msec to 2 seconds
            var max: UInt16

            /// in units of connection intervals, valid range: 0 to 30
            var slaveLatency: UInt8

            /// in units of 100 ms, valid range: 2 seconds to 6 seconds
            var supervisionTimeout: UInt8

            public var description: String {
                return [
                    "min: \(min)",
                    "max: \(max)",
                    "slaveLatency: \(slaveLatency)",
                    "supervisionTimeout: \(supervisionTimeout)"
                ].joined(separator: ",")
            }

            static let dummy = ModeConfiguration(min: 0, max: 0, slaveLatency: 0, supervisionTimeout: 0)

            static let connectionIntervalValidRange = ValidRange<Double, Double>(min: 12, max: 1600, unit: 1.25)
            static let slaveLatencyValidRange = ValidRange<UInt8, UInt8>(min: 0, max: 30, unit: 1)
            static let supervisionTimeoutValidRange = ValidRange<UInt32, UInt32>(min: 20, max: 60, unit: 100)
        }

        public struct DLEConfiguration: CustomStringConvertible, Codable {
            /// in units of bytes, valid range: 0x001B-0x00FB
            var maxTxPDU: UInt8

            /// in units of microseconds, valid range: 0x0148-0x0848
            var maxTxTime: UInt16

            public var description: String {
                let values = [
                    "maxTxPDU: \(maxTxPDU)",
                    "maxTxTime: \(maxTxTime)"
                ]

                return values.joined(separator: ",")
            }

            static let dummy = DLEConfiguration(maxTxPDU: 0, maxTxTime: 0)
        }

        /// Fast Mode Configuration
        public var fastModeConfiguration: ModeConfiguration?

        /// Slow Mode Configuration
        public var slowModeConfiguration: ModeConfiguration?

        /// DLE Configuration
        public var dleConfiguration: DLEConfiguration?

        /// in units of bytes, valid range: 23 - 506
        public var MTU: UInt16?

        public var description: String {
            let values: [String?] = [
                fastModeConfiguration != nil ? "FastMode: \(fastModeConfiguration!)" : nil,
                slowModeConfiguration != nil ? "SlowMode: \(slowModeConfiguration!)" : nil,
                dleConfiguration != nil ? "DLE: \(dleConfiguration!)" : nil,
                MTU != nil ? "MTU: \(MTU!)" : nil
            ]

            return values
                .compactMap { $0 }
                .joined(separator: "|")
        }
    }
}

extension LinkConfigurationService.PreferredConnectionConfiguration.ModeConfiguration: RawRepresentable {
    public init?(rawValue: Data) {
        guard rawValue.count >= 6 else {
            LogBindingsWarning("ModeConfiguration was \(rawValue.count) bytes - expected: 6")
            return nil
        }

        min = rawValue[0...1].asLittleEndian()
        max = rawValue[2...3].asLittleEndian()
        slaveLatency = rawValue[4...4].asLittleEndian()
        supervisionTimeout = rawValue[5...5].asLittleEndian()
    }

    public var rawValue: Data {
        var result = Data()

        result.append(littleEndian: min)
        result.append(littleEndian: max)
        result.append(littleEndian: slaveLatency)
        result.append(littleEndian: supervisionTimeout)

        return result
    }
}

extension LinkConfigurationService.PreferredConnectionConfiguration.DLEConfiguration: RawRepresentable {
    public init?(rawValue: Data) {
        guard rawValue.count >= 3 else {
            LogBindingsWarning("ModeConfiguration was \(rawValue.count) bytes - expected: 3")
            return nil
        }

        maxTxPDU = rawValue[0...0].asLittleEndian()
        maxTxTime = rawValue[1...2].asLittleEndian()
    }

    public var rawValue: Data {
        var result = Data()

        result.append(littleEndian: maxTxPDU)
        result.append(littleEndian: maxTxTime)

        return result
    }
}

extension LinkConfigurationService.PreferredConnectionConfiguration: RawRepresentable {
    public init?(rawValue: Data) {
        guard rawValue.count >= 18 else {
            LogBindingsWarning("PreferredConnectionConfiguration was \(rawValue.count) bytes - expected: 18")
            return nil
        }

        do {
            let mask = Mask(rawValue: rawValue[0])
            fastModeConfiguration = mask.contains(.fastMode) ? ModeConfiguration(rawValue: rawValue.subdata(in: 1..<7)) : nil
            slowModeConfiguration = mask.contains(.slowMode) ? ModeConfiguration(rawValue: rawValue.subdata(in: 7..<13)) : nil
            dleConfiguration = mask.contains(.dle) ? DLEConfiguration(rawValue: rawValue.subdata(in: 13..<16)) : nil
            MTU = mask.contains(.mtu) ? rawValue[16...17].asLittleEndian() : nil
        }
    }

    public var rawValue: Data {
        var mask: Mask = []
        mask.set(.fastMode, fastModeConfiguration != nil)
        mask.set(.slowMode, slowModeConfiguration != nil)
        mask.set(.dle, dleConfiguration != nil)
        mask.set(.mtu, MTU != nil)

        var result = Data()

        result.append(littleEndian: mask.rawValue)
        result.append((fastModeConfiguration ?? ModeConfiguration.dummy).rawValue)
        result.append((slowModeConfiguration ?? ModeConfiguration.dummy).rawValue)
        result.append((dleConfiguration ?? DLEConfiguration.dummy).rawValue)
        result.append(littleEndian: MTU ?? 0)

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
