//
//  WriteRequest.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/30/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation

/// Utility to detect when we forget to respond to CBATTRequests,
/// but will also not send a response if we don't have to.
public class WriteRequest {
    public let central: CBCentral
    public let characteristic: CBCharacteristic
    let value: Data?

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

    func respond(withResult result: CBATTError.Code) {
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
