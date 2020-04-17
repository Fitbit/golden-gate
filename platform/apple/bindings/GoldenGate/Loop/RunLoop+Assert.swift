//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RunLoop+Assert.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/22/18.
//

/// Thread Dictionary key that is used to place and read a sentinel
/// whether the current thread is associated with a run loop.
private let runLoopSentinelKey = "isGoldenGateRunLoop"

/// Utility to check whether the current thread is associated
/// with a run loop.
internal extension Thread {
    var isRunLoop: Bool {
        get {
            return threadDictionary[runLoopSentinelKey] != nil
        }

        set {
            if newValue {
                threadDictionary[runLoopSentinelKey] = true
            } else {
                threadDictionary[runLoopSentinelKey] = nil
            }
        }
    }
}

/// Predicate for usage with `runLoopPrecondition`
internal enum RunLoopPredicate {
    /// Current thread is a run loop
    case onRunLoop
    /// Current thread is not a run loop.
    case notOnRunLoop
}

/// Allows to check whether the current thread is a GoldenGate RunLoop
/// Inspired by `dispatchPrecondition` which we can't use
/// since our `RunLoop is not implemented using a dispatch queue.
internal func runLoopPrecondition(condition: RunLoopPredicate, function: StaticString = #function, file: StaticString = #file) {
    switch condition {
    case .onRunLoop:
        precondition(Thread.current.isRunLoop, "\(file):\(function) MUST \(runLoopPreconditionEpilog())")
    case .notOnRunLoop:
        precondition(!Thread.current.isRunLoop, "\(file):\(function) MUST NOT \(runLoopPreconditionEpilog())")
    }
}

private func runLoopPreconditionEpilog() -> String {
    let label = __dispatch_queue_get_label(nil)
    let name = String(cString: label, encoding: .utf8)!
    var tail = ""

    // Only print the stack trace if the debugger is not attached.
    // Otherwise the DBGLLDBSessionThread of Xcode 9.2 crashes.
    if !isDebuggerAttached() {
        tail = "\n" + Thread.callStackSymbols.joined(separator: "\n")
    }

    return "be called from a GoldenGate run loop, but was called from \(name)\(tail)"
}

/// Utility to determine whether the debugger is attached.
private func isDebuggerAttached() -> Bool {
    // If the debugger is attached, STDERR is a TTY
    return isatty(STDERR_FILENO) != 0
}
