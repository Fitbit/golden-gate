//
//  Connector.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 1/9/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation
import RxSwift

public protocol Connector {
    associatedtype Descriptor
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
    func establishConnection(with descriptor: Descriptor) -> Observable<ConnectionType>
}
