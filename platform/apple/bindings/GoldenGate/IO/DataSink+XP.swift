//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSink+XP.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/10/17.
//

import BluetoothConnection
import Foundation
import GoldenGateXP

/// A `DataSink` that translates the error types from `BluetoothConnection` framework to `GGRawError`.
class GGErrorMappingDataSink: DataSink {
    private var wrapped: DataSink

    init(_ dataSink: DataSink) {
        self.wrapped = dataSink
    }

    func put(_ buffer: Buffer, metadata: DataSink.Metadata?) throws {
        LogBindingsVerbose("\(self).put \(buffer) to \(wrapped)")
        do {
            try wrapped.put(buffer, metadata: metadata)
        } catch BluetoothConnectionError.wouldBlock {
            throw GGRawError.wouldBlock
        }
    }

    func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        try wrapped.setDataSinkListener(dataSinkListener)
    }
}

struct DataSinkConstants {
    static let ggBufferMetadataKey = "GG_BUFFER_METADATA"
}

extension DataSink {
    /// Returns an `UnsafeDataSink` for this `DataSink`.
    var gg: UnsafeDataSink {
        if let unsafe = self as? UnsafeDataSink {
            return unsafe
        } else {
            return ManagedDataSink(self)
        }
    }
}

/// A `DataSink` that can be passed to C or comes from C.
protocol UnsafeDataSink: DataSink {
    /// The pointer type to a `DataSink`.
    typealias Ref = UnsafeMutablePointer<GG_DataSink>

    /// A pointer to this object.
    var ref: Ref { get }
}

/// Utility to access C `DataSink` objects in Swift.
internal final class UnmanagedDataSink: UnsafeDataSink {
    internal let ref: UnsafeDataSink.Ref

    private var listener: UnsafeDataSinkListener?

    public init(_ ref: UnsafeDataSink.Ref!) {
        self.ref = ref
    }

    public func put(_ buffer: Buffer, metadata: Metadata?) throws {
        let unsafeBuffer = buffer.gg

        let xpMetadata = metadata?[DataSinkConstants.ggBufferMetadataKey] as? UnsafePointer<GG_BufferMetadata> ?? nil

        // Ensure that buffer stays at least valid until
        // `GG_DataSink_PutData` returns - or longer
        // if someone eventually calls `GG_Buffer_Retain()`.
        try withExtendedLifetime(unsafeBuffer) {
            try GG_DataSink_PutData(ref, unsafeBuffer.ref, xpMetadata).rethrow()
        }
    }

    public func setDataSinkListener(_ listener: DataSinkListener?) throws {
        self.listener = listener?.gg.weakify()
        try GG_DataSink_SetListener(ref, self.listener?.ref).rethrow()
    }
}

/// Utility to access Swift `DataSink` objects in C.
public final class ManagedDataSink: GGAdaptable, UnsafeDataSink {
    typealias GGObject = GG_DataSink
    typealias GGInterface = GG_DataSinkInterface

    private let instance: DataSink
    internal let adapter: Adapter

    private static var interface = GG_DataSinkInterface(
        PutData: { thisPointer, bufferPointer, metadata in
            .from {
                let `self` = Adapter.takeUnretained(thisPointer)
                try self.put(
                    UnmanagedBuffer(bufferPointer!),
                    metadata: [DataSinkConstants.ggBufferMetadataKey: metadata as Any]
                )
            }
        },
        SetListener: { thisPointer, dataSinkListenerRef in
            .from {
                let `self` = Adapter.takeUnretained(thisPointer)
                if let dataSinkListenerRef = dataSinkListenerRef {
                    try self.setDataSinkListener(UnmanagedDataSinkListener(dataSinkListenerRef))
                } else {
                    try self.setDataSinkListener(nil)
                }
            }
        }
    )

    public init(_ instance: DataSink) {
        assert(!(instance is UnsafeDataSink))
        self.instance = instance
        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    public func put(_ buffer: Buffer, metadata: Metadata?) throws {
        try instance.put(buffer, metadata: metadata)
    }

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        try instance.setDataSinkListener(dataSinkListener)
    }
}
