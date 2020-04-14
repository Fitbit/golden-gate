//
//  CoapOptionPointer.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/29/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import GoldenGateXP

/// Converts an array of CoapOptions to a pointer that points to an array of GG_CoapMessageOptionParam.
///
/// Note: The memory will be deallocated when no one is referencing this object. Never call deallocate on the ref pointer.
class CoapOptionPointer {
    typealias Ref = UnsafeMutablePointer<GG_CoapMessageOptionParam>
    let ref: Ref
    private let options: [CoapOption]

    init(_ options: [CoapOption]) {
        self.options = options
        self.ref = Ref.allocate(capacity: options.count)
        var iterable = ref

        for option in options {
            iterable.pointee = GG_CoapMessageOptionParam(option: option.ref, next: nil)
            iterable = iterable.successor()
        }
    }

    deinit {
        #if swift(>=4.1)
        ref.deallocate()
        #else
        ref.deallocate(capacity: 1)
        #endif
    }
}
