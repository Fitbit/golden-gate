//
//  BluetoothManagerWrapper.swift
//  GoldenGateHost-iOS
//
//  Created by Emanuel Jarnea on 09/09/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import GoldenGate

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
