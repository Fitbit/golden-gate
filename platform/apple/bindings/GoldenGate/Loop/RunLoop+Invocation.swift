//
//  RunLoop+Invocation.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/22/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGateXP

extension RunLoop {
    public func async(execute work: @escaping () -> Void) {
        if !running {
            LogBindingsWarning("\(self) is not running yet")
        }

        let task = AsyncTask(closure: work)

        internalQueue.async {
            do {
                try self.post(task)
            } catch {
                assertLogBindingsError("Could not post task: \(error)")
                task.release?()
            }
        }
    }

    fileprivate func post(_ task: AsyncTask, timeout: GG_Timeout = .infinite) throws {
        try GG_Loop_PostMessage(ref, task.retain().ref, timeout).rethrow()
    }
}

private final class AsyncTask: GGAdaptable {
    typealias GGObject = GG_LoopMessage
    typealias GGInterface = GG_LoopMessageInterface

    private let closure: () -> Void
    fileprivate var release: (() -> Void)?

    let adapter: Adapter

    private static var interface = GG_LoopMessageInterface(
        Handle: { ref in
            Adapter.takeUnretained(ref).closure()
        },
        Release: { ref in
            Adapter.takeUnretained(ref).release?()
        }
    )

    init(closure: @escaping () -> Void) {
        self.closure = closure

        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    fileprivate func retain() -> AsyncTask {
        // Note: This intentionally creates a retain-cycle which is
        // resolved when `release` is called by the loop. 🎩
        release = { self.release = nil }
        return self
    }
}

extension RunLoop {
    /// Invoke a block on the run loop.
    ///
    /// Note: The method can be called from the run loop without the risk of a deadlock.
    /// Explicit throwing version of `sync`.
    /// Sadly there's nothing like `withActuallyOnlyRethrowing`
    /// https://forums.swift.org/t/pitch-rethrows-unchecked/10078
    ///
    /// - Parameter closure: Block that will be executed on the run loop.
    public func sync<T>(execute work: () throws -> T) throws -> T {
        // Note: This hack 🎩 solves two problems:
        // 1. The value needs to be initialized otherwise the compiler will display an error.
        // 2. If the "work" block returns nothing, the "sync" should return Void.
        var value: Any = ()

        // Forward the error thrown by the "work" block on the run loop to the caller.
        var workError: Error?

        invokeSync {
            do {
                value = try work()
            } catch {
                workError = error
            }
        }

        if let error = workError { throw error }

        // swiftlint:disable force_cast
        return value as! T
    }

    /// Invoke a block on the run loop.
    ///
    /// Note: The method can be called from the run loop without the risk of a deadlock.
    /// Explicit non-throwing version of `sync`.
    /// Sadly there's nothing like `withActuallyOnlyRethrowing`
    /// https://forums.swift.org/t/pitch-rethrows-unchecked/10078
    ///
    /// - Parameter closure: Block that will be executed on the run loop.
    public func sync<T>(execute work: () -> T) -> T {
        // swiftlint:disable:next force_try
        return try! sync(execute: { () throws -> T in
            return work()
        })
    }

    /// A reference wrapper that works around the limitation that
    /// values cannot be captured inside @convention(c) blocks.
    private struct BlockOperation {
        private let operation: () throws -> Void

        init(operation: @escaping () throws -> Void) {
            self.operation = operation
        }

        func execute() throws {
            try operation()
        }
    }

    /// Invoke a block on the run loop.
    ///
    /// Note: The method can be called from the run loop without the risk of a deadlock.
    ///
    /// - Parameter closure: Block that will be executed on the run loop.
    private func invokeSync(closure: () throws -> Void) {
        withoutActuallyEscaping(closure) { closure in
            var operation = BlockOperation(operation: closure)

            GG_Loop_InvokeSync(self.ref, { pointer -> Int32 in
                let operation = pointer!.assumingMemoryBound(to: BlockOperation.self)

                do {
                    let blockOperation = operation.pointee
                    try blockOperation.execute()

                    return GG_SUCCESS
                } catch {
                    return GG_FAILURE
                }
            }, &operation, nil)
        }
    }
}
