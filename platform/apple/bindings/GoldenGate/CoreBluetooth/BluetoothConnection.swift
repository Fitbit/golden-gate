//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothConnection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/6/17.
//

import Foundation
import RxCocoa
import RxSwift

public protocol BluetoothConnection: Connection {
    var descriptor: BluetoothPeerDescriptor { get }
}

extension BluetoothConnection {
    public var description: String {
        return "\(type(of: self))(descriptor=\(descriptor), link=\(networkLinkStatus.value))"
    }
}
