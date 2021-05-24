//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothManagerWrapper.swift
//  GoldenGateHost-iOS
//
//  Created by Emanuel Jarnea on 09/09/2019.
//

import GoldenGate

public protocol BluetoothManagerType {
    ///  Get bluetooth power status.
    ///
    /// - Returns: true, if bluetooth is powered, or false, otherwise.
    /// - Throws: An error when the status couldn't be read.
    func isPowered() throws -> Bool

    ///  Turn on/off bluetooth radio.
    ///
    /// - Parameter _: true - turn on bluetooth radio
    ///                false - turn off bluetooth radio.
    /// - Throws: An error when the new state couldn't be set.
    func setPowered(_: Bool) throws

    ///  Get bluetooth blacklist enabled status. New bluetooth connections are not allowed
    ///  when blacklist is enabled.
    ///  Note: Bluetooth blacklist can be toggled from Control center.
    ///
    /// - Returns: true, if bluetooth blacklist in not enabled, or false, otherwise.
    /// - Throws: An error when the status couldn't be read.
    func isBlacklistEnabled() throws -> Bool

    ///  Set bluetooth blacklist enabled status.
    ///
    /// - Parameter _: true - enable bluetooth blacklist
    ///                false - disable bluetooth blacklist.
    /// - Throws: An error when the new state couldn't be set.
    func setBlacklistEnabled(_: Bool) throws
}

/// Wrapper over the private BluetoothManager class. This class must never be released to production.
public final class BluetoothManagerWrapper: BluetoothManagerType {
    private let frameworkPath = "/System/Library/PrivateFrameworks/BluetoothManager.framework"
    private var wrapped: Swift.Result<NSObject, Error> = .failure(.notReady)

    public init() {
        guard let bundle = Bundle(path: frameworkPath) else {
            self.wrapped = .failure(.frameworkNotFound)
            return
        }

        guard bundle.isLoaded || bundle.load() else {
            self.wrapped = .failure(.frameworkNotLoaded)
            return
        }

        guard let wrappedClass = NSClassFromString("BluetoothManager") else {
            self.wrapped = .failure(.classNotFound)
            return
        }

        guard let wrappedInstance = wrappedClass.value(forKey: "sharedInstance") as? NSObject else {
            self.wrapped = .failure(.instanceNotFound)
            return
        }

        // For some undocumented(!) reason, we need to wait a bit before BluetoothManager is ready to use.
        DispatchQueue.main.async {
            self.wrapped = .success(wrappedInstance)
        }
    }

    public func isPowered() throws -> Bool {
        return try value(forKey: "powered")
    }

    public func setPowered(_ powered: Bool) throws {
        try setValue(powered, forKey: "powered")
    }

    public func isBlacklistEnabled() throws -> Bool {
        return try value(forKey: "blacklistEnabled")
    }

    public func setBlacklistEnabled(_ enabled: Bool) throws {
        try setValue(enabled, forKey: "blacklistEnabled")
    }

    private func value<T>(forKey key: String) throws -> T {
        guard let value = try wrapped.get().value(forKey: key) as? T else {
            throw Error.typeMismatch(forKey: key)
        }

        return value
    }

    private func setValue<T>(_ value: T, forKey key: String) throws {
        try wrapped.get().setValue(value, forKey: key)
    }
}

private extension BluetoothManagerWrapper {
    enum Error: Swift.Error, CustomStringConvertible {
        case frameworkNotFound
        case frameworkNotLoaded
        case classNotFound
        case instanceNotFound
        case notReady
        case typeMismatch(forKey: String)

        var description: String {
            switch self {
            case .frameworkNotFound:
                return "BluetoothManagerWrapper.Error: BluetoothManager framework not found"
            case .frameworkNotLoaded:
                return "BluetoothManagerWrapper.Error: BluetoothManager framework not loaded"
            case .classNotFound:
                return "BluetoothManagerWrapper.Error: BluetoothManager class not found"
            case .instanceNotFound:
                return "BluetoothManagerWrapper.Error: BluetoothManager shared instance not found"
            case .notReady:
                return "BluetoothManagerWrapper.Error: BluetoothManager shared instance not ready to use"
            case .typeMismatch(let forKey):
                return "BluetoothManagerWrapper.Error: BluetoothManager key '\(forKey)' type mismatch"
            }
        }
    }
}
