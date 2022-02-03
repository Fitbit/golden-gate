//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ReadRequest.swift
//  BluetoothConnection
//
//  Created by Sylvain Rebaud on 10/21/18.
//

import CoreBluetooth
import Foundation

/// Abstraction over the mechanism of responding to Bluetooth read requests
public protocol ReadRequestType: AnyObject {
    var readerIdentifier: UUID { get }
    func respond(withResult result: ReadRequest.Result)
}

/// Utility to detect when we forget to respond to CBATTRequests.
public final class ReadRequest: ReadRequestType {
    public let central: CBCentral
    public let characteristic: CBCharacteristic

    public var readerIdentifier: UUID { central.identifier }

    public enum Result {
        case success(Data?)
        case failure(CBATTError.Code)
    }

    private var internalRespond: ((Result) -> Void)?

    init(peripheralManager: CBPeripheralManager, request: CBATTRequest) {
        central = request.central
        characteristic = request.characteristic

        if characteristic.properties.contains(.read) {
            internalRespond = { result in
                switch result {
                case .success(let data):
                    request.value = data
                    peripheralManager.respond(to: request, withResult: .success)
                case .failure(let code):
                    assert(code != .success)
                    peripheralManager.respond(to: request, withResult: code)
                }
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
