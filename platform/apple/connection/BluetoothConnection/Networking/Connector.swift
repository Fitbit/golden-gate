//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Connector.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 1/9/18.
//

import Foundation
import RxSwift

public protocol Connector {
    associatedtype ConnectionType

    /// Establishes a connection to a peripheral upon subscription
    /// and cancels it upon disposal.
    ///
    /// - Parameter descriptor: The peer's descriptor that to connect to.
    ///

    /// - Returns: Observable
    ///   - Value: State of the connection attempt changed.
    ///   - Error: If the connection attempt fails,
    ///            if the descriptor can not be associated with a peripheral,
    ///            or once the previously connected peer disconnects.
    ///   - Complete: Never
    func establishConnection(descriptor: PeerDescriptor) -> Observable<ConnectionStatus<ConnectionType>>
}

public enum ConnectorError: Error {
    case disconnected
}
