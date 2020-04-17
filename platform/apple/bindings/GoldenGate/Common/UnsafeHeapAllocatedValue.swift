//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  UnsafeHeapAllocatedValue.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 21/05/2018.
//

/// Wrapper that gives access to a heap allocated pointer to a generic type T.
/// The pointer will be deallocated when the wrapper is deinitialized. The pointer should not be saved
/// and used after the wrapper object has been destroyed.
class UnsafeHeapAllocatedValue<T> {
    let pointer: UnsafeMutablePointer<T>!

    var value: T! {
        return pointer?.pointee
    }

    init(_ value: T?) {
        guard let value = value else {
            self.pointer = nil
            return
        }

        let pointer = UnsafeMutablePointer<T>.allocate(capacity: 1)
        pointer.pointee = value
        self.pointer = pointer
    }

    deinit {
        pointer?.deallocate()
    }
}
