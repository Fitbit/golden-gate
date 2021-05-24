//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ConnectionStatus.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 05/10/2020.
//

import RxSwift

/// Current state of the connection intent.
public enum ConnectionStatus<Connection>: Equatable {
    /// Not trying to connect at the moment.
    case disconnected

    /// Trying to find and connect to the device at the moment.
    case connecting

    /// Connected to the given device with the given connection at the moment.
    case connected(Connection)

    public static func == (lhs: ConnectionStatus<Connection>, rhs: ConnectionStatus<Connection>) -> Bool {
        switch (lhs, rhs) {
        case (disconnected, disconnected), (connecting, connecting), (connected, connected):
            return true
        default:
            return false
        }
    }
}

public typealias AnyConnectionStatus = ConnectionStatus<Any>

extension AnyConnectionStatus {
    public static let connected = Self.connected(())
}

public extension ConnectionStatus {
    /// Returns the same status, mapping the connection value using the given transformation.
    ///
    /// - Parameter transform: A closure that takes the connection value of this instance.
    /// - Returns: A `ConnectionStatus` instance with the result of evaluating `transform`
    ///   as the new connection value if this instance represents a `connected` status.
    func map<NewConnection>(_ transform: (Connection) -> NewConnection) -> ConnectionStatus<NewConnection> {
        switch self {
        case .disconnected:
            return .disconnected
        case .connecting:
            return .connecting
        case .connected(let connection):
            return .connected(transform(connection))
        }
    }

    /// Returns an observable with the same status, mapping the connection value using the given
    /// transformation.
    ///
    /// - Parameter transform: A closure that takes the connection value of this instance.
    /// - Returns: An observable of `ConnectionStatus` with the result of evaluating `transform`
    ///   as the new connection value if this instance represents a `connected` status.
    func mapObservable<NewConnection>(
        _ transform: @escaping (Connection) throws -> Observable<NewConnection>
    ) -> Observable<ConnectionStatus<NewConnection>> {
        Observable.deferred {
            switch self {
            case .disconnected:
                return .just(.disconnected)
            case .connecting:
                return .just(.connecting)
            case .connected(let connection):
                return try transform(connection)
                    .map { .connected($0) }
            }
        }
    }

    /// Returns the same status, mapping the connection value to `Any`.
    func eraseType() -> AnyConnectionStatus {
        map { $0 as Any }
    }

    var connected: Bool {
        connection != nil
    }

    var connection: Connection? {
        switch self {
        case .connected(let connection): return connection
        case .disconnected, .connecting: return nil
        }
    }
}

extension ConnectionStatus: CustomStringConvertible {
    public var description: String {
        switch self {
        case .disconnected: return "disconnected"
        case .connecting: return "connecting..."
        case .connected(let connection): return "connected (connection=\(connection))"
        }
    }
}
