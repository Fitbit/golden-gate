//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Buffer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/25/18.
//

import BluetoothConnection
import GoldenGateXP

public typealias Port = BluetoothConnection.Port

extension Buffer {
    /// Returns an `UnsafeBuffer` for this `DataSink`.
    var gg: UnsafeBuffer {
        if let unsafe = self as? UnsafeBuffer {
            return unsafe
        } else {
            return ManagedBuffer(self)
        }
    }
}

/// A `Buffer` that can be passed to C or comes from C.
protocol UnsafeBuffer: Buffer {
    /// The pointer type to a `Buffer`.
    typealias Ref = UnsafeMutablePointer<GG_Buffer>

    /// A pointer to this object.
    var ref: Ref { get }
}

internal final class UnmanagedBuffer: UnsafeBuffer {
    typealias Ref = UnsafeMutablePointer<GG_Buffer>
    let ref: Ref

    init(_ ref: Ref!) {
        self.ref = ref
        GG_Buffer_Retain(ref)
    }

    ///
    convenience init(_ ref: UnsafeMutablePointer<GG_StaticBuffer>) {
        // NOTE: GG_StaticBuffer_AsBuffer is a macro that uses GG_Cast
        self.init(ref.withMemoryRebound(to: GG_Buffer.self, capacity: 1) { $0 })
    }

    deinit {
        GG_Buffer_Release(ref)
    }

    var count: Int {
        return GG_Buffer_GetDataSize(ref)
    }

    var data: Data {
        return Data(bytes: GG_Buffer_GetData(ref)!, count: count)
    }

    func getRawData() -> UnsafePointer<UInt8> {
        return GG_Buffer_GetData(ref)!
    }

    func useRawData() -> UnsafeMutablePointer<UInt8>! {
        return GG_Buffer_UseData(ref)
    }
}

/// Wraps buffers that are implemented in Swift so that
/// they can be used in C.
final class ManagedBuffer: GGAdaptable, UnsafeBuffer {
    typealias GGObject = GG_Buffer
    typealias GGInterface = GG_BufferInterface

    private let instance: Buffer
    let adapter: Adapter

    private static var interface = GG_BufferInterface(
        Retain: { ref in
            return Adapter.takeUnretained(ref).passRetained()
        },
        Release: { ref in
            let instance = Adapter.takeUnretained(ref)
            Unmanaged.passUnretained(instance).release()
        },
        GetData: { ref in
            return Adapter.takeUnretained(ref).getRawData()
        },
        UseData: { ref in
            return Adapter.takeUnretained(ref).useRawData()
        },
        GetDataSize: { ref in
            return Adapter.takeUnretained(ref).count
        }
    )

    fileprivate init(_ instance: Buffer) {
        assert(!(instance is UnsafeBuffer))
        self.instance = instance
        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    /// Creates a new managed buffer backed by the given data.
    public convenience init(data: Data) {
        self.init(NSDataBuffer(data as NSData))
    }

    var count: Int {
        return instance.count
    }

    var data: Data {
        return instance.data
    }

    func getRawData() -> UnsafePointer<UInt8> {
        return instance.getRawData()
    }

    func useRawData() -> UnsafeMutablePointer<UInt8>! {
        return instance.useRawData()
    }

    /// Increments the retain counter by one.
    ///
    /// Useful when returning a buffer to C.
    ///
    /// - Note: In ARC, this would normally be solved via autorelease
    func passRetained() -> Ref {
        _ = Unmanaged.passRetained(self)
        return ref
    }
}
