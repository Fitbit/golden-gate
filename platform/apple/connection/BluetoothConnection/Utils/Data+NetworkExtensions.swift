//
//  Data+NetworkExtensions.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation

public extension Data {
    mutating func append<T: FixedWidthInteger>(littleEndian value: T) {
        var mutableValue = value.littleEndian
        withUnsafePointer(to: &mutableValue) { append(UnsafeBufferPointer(start: $0, count: 1)) }
    }
    
    mutating func append<T: FixedWidthInteger>(bigEndian value: T) {
        var mutableValue = value.bigEndian
        withUnsafePointer(to: &mutableValue) { append(UnsafeBufferPointer(start: $0, count: 1)) }
    }
}

extension Array where Element: FixedWidthInteger {
    public init(bigEndian data: Data) {
        assert(data.count % MemoryLayout<Element>.size == 0, "Invalid number of bytes for conversion")

        self = data.withUnsafeBytes {
            UnsafeBufferPointer<Element>(start: $0.baseAddress?.assumingMemoryBound(to: Element.self),
                                         count: data.count / MemoryLayout<Element>.size)
                .map(Element.init(bigEndian:))
        }
    }

    public init(littleEndian data: Data) {
        assert(data.count % MemoryLayout<Element>.size == 0, "Invalid number of bytes for conversion")

        self = data.withUnsafeBytes {
            UnsafeBufferPointer<Element>(start: $0.baseAddress?.assumingMemoryBound(to: Element.self),
                                         count: data.count / MemoryLayout<Element>.size)
                .map(Element.init(littleEndian:))
        }
    }
}

public extension Data {
    func asBigEndian<T>() -> T where T: FixedWidthInteger {
        return asBigEndians()[0]
    }

    func asLittleEndian<T>() -> T where T: FixedWidthInteger {
        return asLittleEndians()[0]
    }

    func asBigEndians<T>() -> [T] where T: FixedWidthInteger {
        return [T](bigEndian: self)
    }

    func asLittleEndians<T>() -> [T] where T: FixedWidthInteger {
        return [T](littleEndian: self)
    }
}

public extension FixedWidthInteger {
    func toLittleEndian() -> Data {
        var data = Data()
        data.append(littleEndian: self)
        return data
    }

    func toBigEndian() -> Data {
        var data = Data()
        data.append(bigEndian: self)
        return data
    }
}
