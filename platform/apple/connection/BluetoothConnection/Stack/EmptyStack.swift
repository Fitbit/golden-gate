//
//  EmptyStack.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 22/06/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import Foundation

/// A stack that forwards data, without modifications, from the top port to the network link
/// and vice versa. Such a stack is useful in situations where no stack layer is available.
///
/// NOTE: An optimization is made to avoid unnecessarily passing the data through stack's top
/// and bottom ports: the top port is the same as the network link. Thus, application level protocols
/// directly exchange data with the network link.
public final class EmptyStack: StackType {
    public let topPort: Port?

    public init(link: NetworkLink) {
        topPort = link
    }

    public func start() {}

    public func destroy() {}
}
