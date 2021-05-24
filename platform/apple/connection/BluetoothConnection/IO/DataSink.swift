//
//  DataSink.swift
//  GoldenGate
//
//  Created by Vlad Corneci on 02/06/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import Foundation

/// An object designed to receive incoming data.
public protocol DataSink: class {
    typealias Metadata = [String: Any]

    func put(_ buffer: Buffer, metadata: Metadata?) throws

    /// - Warning: The listener will be referenced weakly to avoid ref-cycles.
    func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws
}

/// Utilities
public extension DataSink {
    func put(_ buffer: Buffer) throws {
        try put(buffer, metadata: nil)
    }

    func put(_ data: Data, metadata: Metadata? = nil) throws {
        try put(NSDataBuffer(data as NSData), metadata: metadata)
    }
}
