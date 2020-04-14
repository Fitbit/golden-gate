//
//  StackConfiguration.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGateXP

/// Utility that holds strong references to objects
/// used by the stack builder.
public struct StackConfiguration {
    /// The role of the device
    public let role: Role

    /// The descriptor of the stack.
    public let descriptor: StackDescriptor

    // Properties that are accessible via `withParameters`
    private let gattlinkParameters: GattlinkParametersContainer?
    private let dtlsClientParameters: SecureDatagramClient.OptionsContainer?
    private let dtlsServerParameters: SecureDatagramServer.OptionsContainer?
    
    init(
        role: Role,
        descriptor: StackDescriptor,
        gattlinkParameters: GattlinkParameters?,
        dtlsClientParameters: SecureDatagramClient.Options?,
        dtlsServerParameters: SecureDatagramServer.Options?
    ) {
        self.role = role
        self.descriptor = descriptor
        self.gattlinkParameters = gattlinkParameters.map(GattlinkParametersContainer.init(gattlinkParameters:))
        self.dtlsClientParameters = dtlsClientParameters.map(SecureDatagramClient.OptionsContainer.init(options:))
        self.dtlsServerParameters = dtlsServerParameters.map(SecureDatagramServer.OptionsContainer.init(options:))
    }

    /// Utility that prevents the memory backing the parameters
    /// from being freed during the execution of the closure.
    ///
    /// - See also: `Data.withUnsafeMutableBytes`
    internal mutating func withParameters<T>(closure: ([GG_StackBuilderParameters]) throws -> T) rethrows -> T {
        var parameters = [GG_StackBuilderParameters]()

        if let gg = dtlsClientParameters.map({ $0.ref }) {
            parameters.append(GG_StackBuilderParameters(type: .dtlsClient, value: gg))
        }

        if let gg = dtlsServerParameters.map({ $0.ref }) {
            parameters.append(GG_StackBuilderParameters(type: .dtlsServer, value: gg))
        }

        if let gg = gattlinkParameters.map({ $0.ref }) {
            parameters.append(GG_StackBuilderParameters(type: .gattlink, value: gg))
        }

        return try closure(parameters)
    }
}

extension StackConfiguration {
    /// IoC utility that only asks for configuration on demand
    init(
        role: Role,
        descriptor: StackDescriptor,
        networkLinkMtu: UInt,
        configurationProvider: StackElementConfigurationProvider
    ) {
        var dtlsClientParameters: SecureDatagramClient.Options?
        var dtlsServerParameters: SecureDatagramServer.Options?
        var gattlinkParameters: GattlinkParameters?

        // Load the configuration if DTLS is used
        if descriptor.hasDtls {
            switch role {
            case .hub:
                dtlsServerParameters = configurationProvider.dtlsServerConfiguration()
            case .node:
                dtlsClientParameters = configurationProvider.dtlsClientConfiguration()
            }
        }

        // Load the configuration if Gattlink is used
        if descriptor.hasGattlink {
            let defaultParameters = configurationProvider.gattlinkConfiguration() ?? GattlinkParameters()
            gattlinkParameters = GattlinkParameters(
                rxWindow: defaultParameters.rxWindow,
                txWindow: defaultParameters.txWindow,
                bufferSize: defaultParameters.bufferSize,
                initialMaxFragmentSize: UInt16(clamping: networkLinkMtu),
                probeConfig: defaultParameters.probeConfig
            )
        }

        self.init(
            role: role,
            descriptor: descriptor,
            gattlinkParameters: gattlinkParameters,
            dtlsClientParameters: dtlsClientParameters,
            dtlsServerParameters: dtlsServerParameters
        )
    }
}

/// Provider for on-demand StackElement configuration
public protocol StackElementConfigurationProvider {
    /// Query DTLS server configuration
    ///
    /// - Returns: The DTLS server configuration
    ///     or `nil`, if DTLS server mode is not supported.
    func dtlsServerConfiguration() -> SecureDatagramServer.Options?

    /// Query DTLS client configuration
    ///
    /// - Returns: The DTLS server configuration
    ///     or `nil`, if DTLS client mode is not supported.
    func dtlsClientConfiguration() -> SecureDatagramClient.Options?

    /// Query Gattlink configuration
    ///
    /// - Returns: The Gattlink configuration
    ///     or `nil`, if the default configuration should be used.
    func gattlinkConfiguration() -> GattlinkParameters?
}

private extension GG_StackBuilderParameters {
    /// Utility to use `StackElementType` directly
    init(type: StackElementType, value ref: UnsafeRawPointer) {
        self.init(
            element_type: type.rawValue.rawValue,
            element_parameters: ref
        )
    }
}
