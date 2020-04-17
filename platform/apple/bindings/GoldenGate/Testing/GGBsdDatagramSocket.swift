//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGBsdDatagramSocket.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/9/18.
//

import GoldenGateXP

/// A platform native BSD datagram socket
///
/// Useful to test higher-level components (e.g. blaster or CoAP)
/// with a service on the IP network, that the device is connected to,
/// instead of the virtual GoldenGate network.
///
/// - See also: GG_BsdDatagramSocket
public class GGBsdDatagramSocket: DataSink, DataSource {
    typealias Ref = UnsafeMutablePointer<GG_DatagramSocket>
    private let ref: Ref

    /// Creates a native BSD Datagram Socket
    ///
    /// - Parameters:
    ///   - runLoop: The run loop to attach this socket to.
    ///   - localAddress: The local address to bind to.
    ///   - remoteAddress: The remote address to connect to.
    ///   - connectToRemote: Whether to call `connect()` on the socket with the `remote_address`.
    ///       Doesn't make a real difference though, as `remote_address` will be passed explicitly
    ///       during `_PutData` as well.
    ///   - maxDatagramSize: The maximum size of a datagram that can be sent or received.
    ///
    /// - SeeAlso: GG_BsdDatagramSocket_Create
    public init(
        runLoop: RunLoop,
        localAddress: SocketAddress?,
        remoteAddress: SocketAddress?,
        connectToRemote: Bool = false,
        maxDatagramSize: Int = 1280
    ) throws {
        self.ref = try withExtendedLifetime(UnsafeHeapAllocatedValue(localAddress?.gg)) { localAddressValue in
            try withExtendedLifetime(UnsafeHeapAllocatedValue(remoteAddress?.gg)) { remoteAddressValue in
                var ref: Ref?
                try GG_BsdDatagramSocket_Create(
                    localAddressValue.pointer,
                    remoteAddressValue.pointer,
                    connectToRemote,
                    UInt32(maxDatagramSize),
                    &ref
                ).rethrow()
                return ref!
            }
        }

        do {
            try GG_DatagramSocket_Attach(ref, runLoop.ref).rethrow()
        } catch {
            GG_DatagramSocket_Destroy(ref)
            throw error
        }
    }

    deinit {
        GG_DatagramSocket_Destroy(ref)
    }

    // MARK: - DataSource

    private lazy var asUnmanagedDataSource: UnmanagedDataSource = {
        return UnmanagedDataSource(GG_DatagramSocket_AsDataSource(ref))
    }()

    public func setDataSink(_ dataSink: DataSink?) throws {
        try asUnmanagedDataSource.setDataSink(dataSink)
    }

    // MARK: - DataSink

    private lazy var asUnmanagedDataSink: UnmanagedDataSink = {
        return UnmanagedDataSink(GG_DatagramSocket_AsDataSink(ref))
    }()

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        try asUnmanagedDataSink.setDataSinkListener(dataSinkListener)
    }

    public func put(_ buffer: Buffer, metadata: DataSink.Metadata?) throws {
        try asUnmanagedDataSink.put(buffer, metadata: metadata)
    }
}

/// Allow using the GGBsdDatagramSocket as a Port
extension GGBsdDatagramSocket: Port {
    public var dataSink: DataSink {
        return self
    }

    public var dataSource: DataSource {
        return self
    }
}
