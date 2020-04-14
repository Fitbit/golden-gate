//
//  GGAdapter.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 10/23/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import GoldenGateXP

/// Utility to work with the vtable mechanism of the XP library.
///
/// - Note: Using `struct` for a (hopefully) predictable memory layout.
///
/// - Warning: You must call `.bind(to:)` after creating this object!
public class GGAdapter<Object, Interface, Implementation: AnyObject> {
    /// The type of the `base` field.
    ///
    /// Alias to prevent programming errors when writing unsafe code further below.
    fileprivate typealias ImplementationRef = UnsafePointer<Implementation>

    internal typealias InterfaceRef = UnsafePointer<Interface>

    public typealias Ref = UnsafeMutablePointer<Object>

    private let numberOfPointers = 2

    /// Heap allocated memory with
    /// - a pointer to the Swift object, and
    /// - a pointer to the `vtable` (similar shape as the `Object`).
    private var pointers: UnsafeMutablePointer<UnsafeRawPointer?>

    /// Creates a new Adapter.
    ///
    /// - Parameters:
    ///   - iface: The interface that is being implemented.
    ///     This is named `iface` to be consistent with the field in XP.
    internal init(iface: InterfaceRef) {
        self.pointers = UnsafeMutablePointer<UnsafeRawPointer?>.allocate(capacity: 2)
        self.pointers.initialize(repeating: nil, count: numberOfPointers)
        self.interfaceRef = iface
    }

    deinit {
        pointers.deallocate()
    }

    /// Simple typed access to a pointer from `pointers`.
    private subscript<T>(index: Int) -> UnsafePointer<T>! {
        get {
            assert(index < numberOfPointers)
            return pointers.advanced(by: index).pointee?.assumingMemoryBound(to: T.self)
        }

        set {
            assert(index < numberOfPointers)
            pointers.advanced(by: index).pointee = UnsafeRawPointer(newValue)
        }
    }

    /// Pointer to the actual Swift instance implementing `Interface`.
    fileprivate var implementationRef: ImplementationRef? {
        get {
            return self[0]
        }
        set {
            self[0] = newValue
        }
    }

    /// Pointer to the GoldenGate `GG_Interface`.
    fileprivate var interfaceRef: InterfaceRef? {
        get {
            return self[1]
        }
        set {
            self[1] = newValue
        }
    }

    /// Bind to an instance implementing the `Interface`.
    ///
    /// - See also: GG_SET_INTERFACE
    ///
    /// - Note:
    ///     Allows late binding of `implementation`.
    ///     Since the adapter generally has to point to `self`
    ///     This avoids the need of an IUO in the class using the adapter,
    ///     as generally
    @discardableResult
    public func bind(to implementation: Implementation) -> GGAdapter {
        assert(implementationRef == nil)

        implementationRef = UnsafePointer(
            Unmanaged
                .passUnretained(implementation)
                .toOpaque()
                .assumingMemoryBound(to: Implementation.self)
        )

        return self
    }
}

public extension GGAdapter {
    /// Returns the object referenced by the `base` field before an `object` field.
    ///
    /// - See also: `GG_SELF`
    static func takeUnretained(_ ref: UnsafePointer<Object>!) -> Implementation {
        // Find the pointer to the Swift instance,
        // which is immediately in front of the object.
        let pointer = UnsafeRawPointer(ref!)
            .assumingMemoryBound(to: UnsafeRawPointer?.self)
            .advanced(by: -1)

        guard let pointee = pointer.pointee else {
            fatalError("Call `bind(to:)` before passing the reference around!")
        }

        return Unmanaged<Implementation>.fromOpaque(pointee).takeUnretainedValue()
    }

    /// Returns a pointer to the `object` field.
    ///
    /// - See also: `GG_CAST`
    static func passUnretained(_ adapter: GGAdapter) -> Ref {
        precondition(adapter.implementationRef != nil, "Call `bind(to:)` before passing the reference around!")

        // While we never actually allocated an instance of "Object"
        // this pointer will point to memory which is just a pointer
        // to an interface.
        return UnsafeMutableRawPointer(adapter.pointers.advanced(by: 1))
            .assumingMemoryBound(to: Object.self)
    }
}

// Private exposure for code coverage
internal extension GGAdapter {
    func unbind() -> GGAdapter {
        assert(interfaceRef != nil)

        implementationRef = nil
        return self
    }
}
