//
//  VoidDataSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/27/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation

/// The `/dev/null` of data sinks
public class VoidDataSink: DataSink {
    private init() {
        // nothing
    }

    public func put(_ buffer: Buffer, metadata: Metadata?) throws {
        // ignore
    }

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        // ignore
    }

    public static let instance = VoidDataSink()
}
