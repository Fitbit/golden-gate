//
//  AnyConnectionController.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 10.02.2022.
//  Copyright © 2022 Fitbit. All rights reserved.
//

import Foundation
import RxSwift

/// A type-erased connection controller. An instance of AnyConnectionController forwards its operations to an underlying
/// base controller having the same ConnectionType type, hiding the specifics of the underlying controller.
public final class AnyConnectionController<ConnectionType>: ConnectionControllerType, Instrumentable, CustomStringConvertible {
    public let descriptor: Observable<PeerDescriptor?>

    public let connectionStatus: Observable<ConnectionStatus<ConnectionType>>
    public let connectionError: Observable<Error?>

    public let metrics: Observable<ConnectionControllerMetricsEvent>

    public init<C: ConnectionControllerType & Instrumentable>(
        connectionController: C
    ) where C.ConnectionType == ConnectionType, C.Event == ConnectionControllerMetricsEvent {
        descriptor = connectionController.descriptor
        connectionStatus = connectionController.connectionStatus
        connectionError = connectionController.connectionError
        metrics = connectionController.metrics

        baseClearDescriptor = connectionController.clearDescriptor
        baseUpdateDescriptor = connectionController.update(descriptor:)
        baseEstablishConnection = connectionController.establishConnection(trigger:)
        baseDisconnect = connectionController.disconnect(trigger:)
        baseDescription = { String(describing: connectionController) }
    }

    private let baseClearDescriptor: () -> Void
    public func clearDescriptor() {
        baseClearDescriptor()
    }

    private let baseUpdateDescriptor: (PeerDescriptor) -> Void
    public func update(descriptor: PeerDescriptor) {
        baseUpdateDescriptor(descriptor)
    }

    private let baseEstablishConnection: (ConnectionTrigger) -> Void
    public func establishConnection(trigger: ConnectionTrigger) {
        baseEstablishConnection(trigger)
    }

    private let baseDisconnect: (ConnectionTrigger) -> Void
    public func disconnect(trigger: ConnectionTrigger) {
        baseDisconnect(trigger)
    }

    private let baseDescription: () -> String
    public var description: String {
        baseDescription()
    }
}
