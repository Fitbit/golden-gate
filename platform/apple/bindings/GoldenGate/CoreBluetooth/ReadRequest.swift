//
//  WriteRequest.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/21/18.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation

/// Utility to detect when we forget to respond to CBATTRequests.
public class ReadRequest {
    public let central: CBCentral
    public let characteristic: CBCharacteristic

    public enum Result {
        case success(Data?)
        case failure(CBATTError.Code)

        func respond(to request: CBATTRequest, on peripheralManager: CBPeripheralManager) {
            switch self {
            case .success(let data):
                request.value = data
                peripheralManager.respond(to: request, withResult: .success)
            case .failure(let code):
                assert(code != .success)
                peripheralManager.respond(to: request, withResult: code)
            }
        }
    }

    private var internalRespond: ((Result) -> Void)?

    init(peripheralManager: CBPeripheralManager, request: CBATTRequest) {
        central = request.central
        characteristic = request.characteristic

        if characteristic.properties.contains(.read) {
            internalRespond = { result in
                result.respond(to: request, on: peripheralManager)
            }
        }
    }

    public func respond(withResult result: Result) {
        internalRespond?(result)
        internalRespond = nil
    }

    deinit {
        if let internalRespond = internalRespond {
            LogBluetoothWarning("\(self) needs a response but was not responded to.")
            internalRespond(.failure(.readNotPermitted))
        }
    }
}

extension ReadRequest: CustomStringConvertible {
    public var description: String {
        return "\(type(of: self))(from=\(central), on=\(characteristic))"
    }
}
