//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSource.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 10/24/17.
//

import Foundation
import GoldenGateXP

public protocol DataSource: class {
    /// Sets the sink to which this source writes data to.
    func setDataSink(_ dataSink: DataSink?) throws
}

extension DataSource {
    /// Returns an `UnsafeDataSource` for this `DataSource`.
    var gg: UnsafeDataSource {
        if let unsafe = self as? UnsafeDataSource {
            return unsafe
        } else {
            return ManagedDataSource(self)
        }
    }
}

/// A `DataSource` that can be passed to C or comes from C.
protocol UnsafeDataSource: DataSource {
    /// The pointer type to a `DataSource`.
    typealias Ref = UnsafeMutablePointer<GG_DataSource>

    /// A pointer to this object.
    var ref: UnsafeDataSource.Ref { get }
}

/// Utility to access C `DataSource` objects in Swift.
class UnmanagedDataSource: UnsafeDataSource {
    let ref: UnsafeDataSource.Ref

    // Strong reference in case the DataSink is implemented in C and
    // can't perform memory management using the Swift runtime.
    private var dataSink: UnsafeDataSink?

    init(_ ref: UnsafeDataSource.Ref!) {
        self.ref = ref
    }

    func setDataSink(_ dataSink: DataSink?) throws {
        self.dataSink = dataSink?.gg
        GG_DataSource_SetDataSink(ref, self.dataSink?.ref)
    }
}

/// Utility to access Swift `DataSource` objects in C.
public final class ManagedDataSource: GGAdaptable, UnsafeDataSource {
    typealias GGObject = GG_DataSource
    typealias GGInterface = GG_DataSourceInterface

    private let instance: DataSource
    internal let adapter: Adapter

    private static var interface = GG_DataSourceInterface(
        SetDataSink: { thisPointer, dataSinkRef in
            .from {
                let `self` = Adapter.takeUnretained(thisPointer)
                if let dataSinkRef = dataSinkRef {
                    try self.setDataSink(UnmanagedDataSink(dataSinkRef))
                } else {
                    try self.setDataSink(nil)
                }
            }
        }
    )

    public init(_ instance: DataSource) {
        assert(!(instance is UnsafeDataSource))
        self.instance = instance

        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    public func setDataSink(_ dataSink: DataSink?) throws {
        try instance.setDataSink(dataSink)
    }
}
