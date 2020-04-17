//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RunLoop+Ownership.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/22/18.
//

import GoldenGateXP
import Rxbit

private extension RunLoop {
    /// Releases the retained unmanaged object on the runloop.
    ///
    /// Useful when the owner of an object wants to ensure
    /// that the object is released on the run loop.
    func releaseRetained<T>(_ object: Unmanaged<T>) {
        self.async {
            object.release()
        }
    }
}

private final class ReleaseTask: GGAdaptable {
    typealias GGObject = GG_LoopMessage
    typealias GGInterface = GG_LoopMessageInterface

    private var object: Any?
    private var release: (() -> Void)?

    let adapter: Adapter

    private static var interface = GG_LoopMessageInterface(
        Handle: {
            Adapter.takeUnretained($0).object = nil
        },
        Release: {
            Adapter.takeUnretained($0).release?()
        }
    )

    init(object: Any) {
        self.object = object

        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    fileprivate func retain() -> ReleaseTask {
        // Note: This intentionally creates a retain-cycle which is
        // resolved when `release` is called by the loop. 🎩
        release = { self.release = nil }
        return self
    }
}

/// Utility to deinit objects on the run loop.
internal class RunLoopGuardedValue<T: AnyObject> {
    private(set) var value: T!
    private let runLoop: RunLoop

    init(_ value: T, runLoop: RunLoop) {
        self.value = value
        self.runLoop = runLoop
    }

    deinit {
        // Keep an unmanaged retained object reference and drop the strong reference
        // so that the only one remaining is the unmanaged one.
        let pointer = Unmanaged.passRetained(value)
        value = nil
        runLoop.releaseRetained(pointer)
    }
}

extension RunLoopGuardedValue: CustomStringConvertible {
    var description: String {
        return "\(type(of: self))(\(value ??? "unknown")))"
    }
}
