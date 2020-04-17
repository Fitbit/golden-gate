//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothRemoteApi.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 09/09/2019.
//

import RxSwift

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

public final class BluetoothRemoteApi: RemoteApiModule {
    private let bluetoothManager: BluetoothManagerType

    public let methods: Set<String> = Method.allUniqueRawValues

    public init(bluetoothManager: BluetoothManagerType) {
        self.bluetoothManager = bluetoothManager
    }

    public func publishHandlers(on remoteShell: RemoteShell) {
        remoteShell.register(Method.enableBt.rawValue, handler: enableBluetooth)
        remoteShell.register(Method.getBtStatus.rawValue, handler: getBluetoothStatus)
    }

    private func enableBluetooth(params: EnableBluetoothParams) -> Completable {
        return .deferred {
            try self.bluetoothManager.setPowered(params.enable)
            try self.bluetoothManager.setBlacklistEnabled(!params.enable)
            return .empty()
        }
    }

    private func getBluetoothStatus() -> Single<Bool> {
        return .deferred {
            let powered = try self.bluetoothManager.isPowered()
            let blacklistEnabled = try self.bluetoothManager.isBlacklistEnabled()
            return .just(powered && !blacklistEnabled)
        }
    }
}

private extension BluetoothRemoteApi {
    enum Method: String, CaseIterable {
        case enableBt       = "bt/enable_bt"
        case getBtStatus    = "bt/get_bt_status"
    }

    struct EnableBluetoothParams: Codable {
        let enable: Bool
    }
}
