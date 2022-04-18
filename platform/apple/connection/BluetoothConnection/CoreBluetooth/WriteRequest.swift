//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  WriteRequest.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 11/30/17.
//

import CoreBluetooth
import Foundation

/// Abstraction over the mechanism of responding to Bluetooth write requests
public protocol WriteRequestType: AnyObject {
    var writerIdentifier: UUID { get }
    func respond(withResult result: CBATTError.Code)
}

/// Utility to detect when we forget to respond to CBATTRequests,
/// but will also not send a response if we don't have to.
public class WriteRequest: WriteRequestType {
    public let central: CBCentral
    public let characteristic: CBCharacteristic
    let value: Data?

    public var writerIdentifier: UUID { central.identifier }

    private var internalRespond: ((CBATTError.Code) -> Void)?

    init(peripheralManager: CBPeripheralManager, request: CBATTRequest) {
        central = request.central
        characteristic = request.characteristic
        value = request.value

        if !characteristic.properties.contains(.writeWithoutResponse) {
            internalRespond = { result in
                peripheralManager.respond(to: request, withResult: result)
            }
        }
    }

    public func respond(withResult result: CBATTError.Code) {
        internalRespond?(result)
        internalRespond = nil
    }

    deinit {
        if let internalRespond = internalRespond {
            LogBluetoothWarning("\(self) needs a response but was not responded to.")
            internalRespond(.writeNotPermitted)
        }
    }
}

extension WriteRequest: CustomStringConvertible {
    public var description: String {
        return "\(type(of: self))(from=\(central), on=\(characteristic), value=\(value ??? "nil"))"
    }
}
