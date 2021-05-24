//
//  Buffer.swift
//  GoldenGate
//
//  Created by Vlad Corneci on 02/06/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import Foundation

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

/// Descriptor for the fullness (the capacity in use) of a buffer relative
/// to some threshold.
public enum BufferFullness: Equatable {
    case underThreshold
    case overThreshold
}

/// A buffer backed by an instance of `NSData`.
///
/// - Note: We use `NSData` so that we can use `data.bytes`.
///   Swift's `Data.withUnsafeBytes` does not guarantee validity
///   of the pointer outside of the closure.
final public class NSDataBuffer: Buffer {
    private let nsData: NSData

    public init(_ nsData: NSData) {
        self.nsData = nsData
    }

    public var count: Int {
        return nsData.length
    }

    public var data: Data {
        return nsData as Data
    }

    public func getRawData() -> UnsafePointer<UInt8> {
        return nsData.bytes.assumingMemoryBound(to: UInt8.self)
    }

    public func useRawData() -> UnsafeMutablePointer<UInt8>! {
        return nil
    }
}
