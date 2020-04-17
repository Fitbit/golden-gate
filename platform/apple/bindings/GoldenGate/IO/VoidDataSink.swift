//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  VoidDataSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/27/17.
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
