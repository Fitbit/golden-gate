//
//  Buffer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/25/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGateXP

/// - See also: GG_Buffer
public protocol Buffer: CustomDebugStringConvertible {
    /// The number of bytes backing this buffer.
    ///
    /// - Note: Might be faster than `data.count`.
    var count: Int { get }

    /// The data backing this buffer.
    var data: Data { get }

    /// Direct read access to the memory backing this buffer.
    func getRawData() -> UnsafePointer<UInt8>

    /// Direct write access to the memory backing this buffer.
    func useRawData() -> UnsafeMutablePointer<UInt8>!
}

extension Buffer {
    /// Returns an `UnsafeBuffer` for this `DataSink`.
    var gg: UnsafeBuffer {
        if let unsafe = self as? UnsafeBuffer {
            return unsafe
        } else {
            return ManagedBuffer(self)
        }
    }

    /// Description containing size and a preview of the buffer's data.
    public var debugDescription: String {
        let count = self.count
        let unit = count == 1 ? "byte" : "bytes"

        let previewLength = min(count, 20)
        let previewBytes = UnsafeBufferPointer(start: getRawData(), count: previewLength)
        let preview = previewBytes.map { String(format: "%02hhx", $0) }.joined()

        let suffix = count > previewLength ? "..." : ""

        return "Buffer(\(count) \(unit), \(preview)\(suffix))"
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

/// A buffer backed by an instance of `NSData`.
///
/// - Note: We use `NSData` so that we can use `data.bytes`.
///   Swift's `Data.withUnsafeBytes` does not guarantee validity
///   of the pointer outside of the closure.
internal class NSDataBuffer: Buffer {
    private let nsData: NSData

    init(_ nsData: NSData) {
        self.nsData = nsData
    }

    var count: Int {
        return nsData.length
    }

    var data: Data {
        return nsData as Data
    }

    func getRawData() -> UnsafePointer<UInt8> {
        return nsData.bytes.assumingMemoryBound(to: UInt8.self)
    }

    func useRawData() -> UnsafeMutablePointer<UInt8>! {
        return nil
    }
}
