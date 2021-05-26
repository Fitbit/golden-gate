//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  LinkConfigurationService+GeneralPurposeCommand.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 22/03/2019.
//

import Foundation

// swiftlint:disable nesting

extension LinkConfigurationService {
    public struct GeneralPurposeCommand: CustomStringConvertible, Codable {
        public enum Code: UInt8, Codable {
            case disconnect = 1
        }

        public var code: Code
        public var parameters: [UInt8]?

        public var description: String {
            return "Code: \(code), parameters: \(parameters ??? "nil")"
        }
    }
}

extension LinkConfigurationService.GeneralPurposeCommand {
    public init(code: Code) {
        self.init(code: code, parameters: nil)
    }
}

extension LinkConfigurationService.GeneralPurposeCommand: RawRepresentable {
    public init?(rawValue: Data) {
        guard rawValue.count >= 1 else {
            LogBluetoothWarning("GeneralPurposeCommand was \(rawValue.count) bytes - expected: >= 1")
            return nil
        }

        let bytes = [UInt8](rawValue)

        do {
            let rawCode = bytes[0]
            if let code = Code(rawValue: rawCode) {
                self.code = code
            } else {
                LogBluetoothWarning("Unknown GeneralPurposeCommand: \(rawCode) (\(String(rawCode, radix: 2)))")
                self.code = .disconnect
            }

            if bytes.count > 1 {
                parameters = Array(bytes[1...])
            }
        }
    }

    public var rawValue: Data {
        return Data([code.rawValue] + (parameters ?? []))
    }
}
