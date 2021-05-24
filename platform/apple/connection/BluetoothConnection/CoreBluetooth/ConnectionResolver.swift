//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ConnectionResolver.swift
//  BluetoothConnection
//
//  Created by Vlad Corneci on 10/08/2020.
//

import RxBluetoothKit
import RxSwift

/// Protocol that attempts to create a connection of a specified type from a list of Bluetooth services and characteristics.
public protocol ConnectionResolver {
    associatedtype ConnectionType

    /// Attempts to create a connection of type `ConnectionType` with the provided Bluetooth services and characteristics.
    /// - Returns: An observable of `ConnectionType` which emits once, but doesn't complete until the connection is torn
    /// down. If the attempt to create the connection fails, the observable errors out with
    /// `ConnectionResolverError.couldNotResolveConnection`.
    func resolveConnection(services: [BluetoothConnector.DiscoveredService], descriptor: PeerDescriptor) -> Observable<ConnectionType>
}

public enum ConnectionResolverError: Error {
    // Error emitted when a resolver can not find the services and characteristics
    // required to create a connection.
    case couldNotResolveConnection
}

/// A Bluetooth connector that attempts to build a specific connection from a list of Bluetooth streams.
public final class ConnectionResolvingConnector<ConnectionType>: Connector {
    private let connector: BluetoothConnector
    private let resolver: ([BluetoothConnector.DiscoveredService], PeerDescriptor) -> Observable<ConnectionType>

    public init<R: ConnectionResolver>(connector: BluetoothConnector, resolver: R) where R.ConnectionType == ConnectionType {
        self.connector = connector
        self.resolver = resolver.resolveConnection
    }

    public func establishConnection(descriptor: PeerDescriptor) -> Observable<ConnectionStatus<ConnectionType>> {
        connector.establishConnection(descriptor: descriptor)
            .flatMapLatest { status -> Observable<ConnectionStatus<ConnectionType>> in
                status.mapObservable { services in
                    self.resolver(services, descriptor)
                }
            }
    }
}
