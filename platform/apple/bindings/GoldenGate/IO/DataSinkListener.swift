//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSinkListener.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 10/24/17.
//

import Foundation
import GoldenGateXP

public protocol DataSinkListener: class {
    /// Called when the DataSink accepts more data after reporting `GG_WOULD_BLOCK`
    func onCanPut()
}

extension DataSinkListener {
    /// Returns an `UnsafeDataSinkListener` for this `DataSinkListener`.
    var gg: UnsafeDataSinkListener {
        if let unsafe = self as? UnsafeDataSinkListener {
            return unsafe
        } else {
            return ManagedDataSinkListener(self)
        }
    }
}

/// A `DataSinkListener` that can be passed to C or comes from C.
protocol UnsafeDataSinkListener: DataSinkListener {
    /// The pointer type to a `DataSinkListener`
    typealias Ref = UnsafeMutablePointer<GG_DataSinkListener>

    /// A pointer to this object.
    var ref: Ref { get }

    /// Returns an `UnsafeDataSinkListener` that holds a weak reference
    /// to the underlying `DataSinkListener`.
    func weakify() -> UnsafeDataSinkListener
}

/// Utility to access C `DataSinkListener` objects in Swift.
class UnmanagedDataSinkListener: UnsafeDataSinkListener {
    let ref: UnsafeDataSinkListener.Ref

    init(_ ref: UnsafeDataSinkListener.Ref!) {
        self.ref = ref
    }

    public func onCanPut() {
        GG_DataSinkListener_OnCanPut(ref)
    }

    func weakify() -> UnsafeDataSinkListener {
        return self
    }
}

/// Utility to access Swift `DataSinkListener` objects in C.
internal final class ManagedDataSinkListener: GGAdaptable, UnsafeDataSinkListener {
    typealias GGObject = GG_DataSinkListener
    typealias GGInterface = GG_DataSinkListenerInterface

    internal let adapter: Adapter
    fileprivate let instance: DataSinkListener

    private static var interface = GG_DataSinkListenerInterface(
        OnCanPut: { thisPointer in
            Adapter.takeUnretained(thisPointer).onCanPut()
        }
    )

    public init(_ instance: DataSinkListener) {
        assert(!(instance is UnsafeDataSinkListener))
        self.instance = instance

        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    public func onCanPut() {
        instance.onCanPut()
    }

    func weakify() -> UnsafeDataSinkListener {
        return WeakManagedDataSinkListener(instance)
    }
}

/// Utility to bridge weak Swift references to C.
private final class WeakManagedDataSinkListener: GGAdaptable, UnsafeDataSinkListener {
    fileprivate weak var instance: DataSinkListener?

    typealias GGObject = GG_DataSinkListener
    typealias GGInterface = GG_DataSinkListenerInterface

    internal let adapter: Adapter

    private static var interface = GG_DataSinkListenerInterface(
        OnCanPut: { thisPointer in
            Adapter.takeUnretained(thisPointer).onCanPut()
        }
    )

    init(_ instance: DataSinkListener) {
        assert(!(instance is UnsafeDataSinkListener))
        self.instance = instance

        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    public func onCanPut() {
        instance?.onCanPut()
    }

    func weakify() -> UnsafeDataSinkListener {
        return self
    }
}
