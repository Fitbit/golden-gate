//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Connection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/30/17.
//

import Foundation
import Rxbit
import RxCocoa
import RxSwift

/// The connection to a GoldenGate peer.
public protocol Connection: class, CustomStringConvertible {
    var networkLinkStatus: BehaviorRelay<NetworkLinkStatus> { get }
}

public enum NetworkLinkStatus {
    case disconnected
    case connecting
    case negotiating(AnyHashable?)
    case connected(NetworkLink)
}

extension NetworkLinkStatus {
    var networkLink: NetworkLink? {
        switch self {
        case .connected(let networkLink): return networkLink
        case .connecting, .disconnected, .negotiating: return nil
        }
    }
}

extension NetworkLinkStatus: CustomStringConvertible {
    public var description: String {
        switch self {
        case .disconnected: return "disconnected"
        case .connecting: return "connecting..."
        case .negotiating(let state): return "negotiating (state=\(state ??? "nil"))"
        case .connected(let networkLink): return "connected (networkLink=\(networkLink ??? "nil"))"
        }
    }
}
