//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NetworkLink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 2/23/18.
//

import Foundation
import RxCocoa

/// Used to create network interfaces.
public protocol NetworkLink: Port {
    var mtu: BehaviorRelay<UInt> { get }
}

extension NetworkLink {
    public var description: String {
        return "\(type(of: self)) mtu=\(mtu.value)"
    }
}
