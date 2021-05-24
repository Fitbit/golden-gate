//
//  Peer.swift
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 01/07/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import Foundation
import RxCocoa
import RxSwift

/// A Bluetooth peer that implements connection management mechanisms, a network
/// stack and various application level protocols running on top of the stack.
public protocol Peer {
    /// The peer's current stack.
    var stack: Observable<StackType?> { get }

    /// The peer's current stack setup.
    var stackDescriptor: Observable<StackDescriptor> { get }

    /// The list of supported stack setups for the current peer type.
    static var supportedStackDescriptors: [StackDescriptor] { get }

    /// The peer's current service setup.
    var serviceDescriptor: Observable<ServiceDescriptor> { get }

    /// The list of supported service setups for the current peer type.
    static var supportedServiceDescriptors: [ServiceDescriptor] { get }

    /// Dictates the port application protocols should use. Useful for
    /// testing the communication over other mediums than Bluetooth.
    var customPortUrl: Observable<URL?> { get }

    /// Emits when the connection is required to terminate in order to recover
    /// from communication failures.
    var disconnectionRequired: Observable<Void> { get }

    /// Emits when the stack buffer fullness changes.
    ///     - The fullness going above a specified threshold indicates important
    ///     activity that could benefit from a faster connection.
    ///     - The fullness going under a specified threshold indicates lower
    ///     activity that could justify using a slower connection.
    var bufferFullness: Observable<BufferFullness> { get }

    /// Configures the peer's stack setup
    func setCustomStackDescriptor(_ descriptor: StackDescriptor?)

     /// Configures the peer's service setup
    func setCustomServiceDescriptor(_ descriptor: ServiceDescriptor?)

     /// Configures the peer's port setup
    func setCustomPortUrl(_ url: URL?)
}

public struct PeerParameters {
    public let stackDescriptor: Observable<StackDescriptor>
    public let serviceDescriptor: Observable<ServiceDescriptor>
    public let defaultPortTransportReadiness: Observable<TransportReadiness>
    public let stackBuilderProvider: (Observable<StackDescriptor>) -> StackBuilderType

    public init(
        stackDescriptor: Observable<StackDescriptor>,
        serviceDescriptor: Observable<ServiceDescriptor>,
        defaultPortTransportReadiness: Observable<TransportReadiness>,
        stackBuilderProvider: @escaping (Observable<StackDescriptor>) -> StackBuilderType
    ) {
        self.stackDescriptor = stackDescriptor
        self.serviceDescriptor = serviceDescriptor
        self.defaultPortTransportReadiness = defaultPortTransportReadiness
        self.stackBuilderProvider = stackBuilderProvider
    }
}
