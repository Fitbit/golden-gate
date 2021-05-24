//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Connection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/30/17.
//

import Foundation
import RxRelay
import RxSwift

/// The connection to a peer.
public protocol Connection: CustomStringConvertible {
    var descriptor: PeerDescriptor { get }
}

extension Connection {
    public var description: String {
        return "\(type(of: self))(descriptor=\(descriptor))"
    }
}

public protocol BondingCapableConnection: Connection {
    var accessBondSecureCharacteristic: Completable { get }
}

public protocol ANCSCapableConnection: Connection {
    var ancsAuthorized: Observable<Bool> { get }
}

public protocol LinkConnection: Connection {
    var networkLink: BehaviorRelay<NetworkLink?> { get }
}

extension LinkConnection {
    public var description: String {
        return "\(type(of: self))(descriptor=\(descriptor), link=\(networkLink.value ??? "nil"))"
    }
}

public typealias SecureLinkConnection = LinkConnection & BondingCapableConnection & ANCSCapableConnection
