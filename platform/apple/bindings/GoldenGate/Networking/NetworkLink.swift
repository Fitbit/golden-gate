//
//  NetworkLink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 2/23/18.
//  Copyright © 2018 Fitbit. All rights reserved.
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
