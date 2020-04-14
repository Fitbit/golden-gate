//
//  RunLoop.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 10/24/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import GoldenGateXP
import RxSwift

public final class RunLoop {
    typealias Ref = OpaquePointer

    /// Whether or not the RunLoop can be terminated.
    ///
    /// Will assert that it never terminates, unless permitted,
    /// so that assumptions that tasks posted to it will always
    /// executed (similarly how GCD crashes if one deallocates a
    /// suspended dispatch queue).
    private let terminable: Bool

    private var thread: Thread?

    internal private(set) var running: Bool = false

    // Note: Swift imports the forward-declared GG_Loop as OpaquePointer.
    internal var ref: OpaquePointer

    internal let internalQueue = DispatchQueue(label: "RunLoop.internalQueue")

    public convenience init(terminable: Bool = true) {
        var ref: Ref?
        GG_Loop_Create(&ref)
        self.init(ref: ref!, terminable: terminable)
    }

    private init(ref: Ref, terminable: Bool = true) {
        self.ref = ref
        self.terminable = terminable
    }

    deinit {
        GG_Loop_Destroy(ref)
    }

    public func start() {
        internalQueue.async {
            guard self.thread == nil else {
                LogBindingsWarning("\(self) was already started.")
                return
            }

            let thread = Thread(target: self, selector: #selector(self.run), object: nil)
            thread.name = "GoldenGate-\(self)"
            self.thread = thread
            thread.start()
        }
    }

    @objc public func run() {
        internalQueue.async {
            precondition(!self.running, "\(self) is running already")
            self.running = true
        }

        Thread.current.isRunLoop = true
        defer { Thread.current.isRunLoop = false }

        do {
            try GG_Loop_Run(ref).rethrow()
        } catch {
            LogBindingsError("RunLoop terminated with \(error)")
        }

        precondition(terminable, "RunLoop terminated unexpectedly")

        internalQueue.async {
            self.running = false

            if Thread.current == self.thread {
                self.thread = nil
            }
        }
    }

    internal func terminate() {
        do {
            try terminate(timeout: .infinite)
        } catch {
            assertLogBindingsError("Termination failed: \(error)")
        }
    }

    internal func terminate(timeout: GG_Timeout) throws {
        guard terminable else { throw GGRawError.notSupported }

        internalQueue.async {
            do {
                let message = GG_Loop_CreateTerminationMessage(self.ref)
                try GG_Loop_PostMessage(self.ref, message, timeout).rethrow()
            } catch {
                assertLogBindingsError("Could not post task: \(error)")
            }
        }
    }

    #if DEBUG
    /// Allows writing tests without worrying about the run loop too much.
    internal static func assumeMainThreadIsRunLoop() {
        Thread.main.isRunLoop = true
    }
    #endif
}

public extension GG_Timeout {
    // Note: GG_TIMEOUT_INFINITE fails to compile because of an overflow warning
    static let infinite = GG_Timeout.max
}

public extension RunLoop {
    static let shared = RunLoop(terminable: false)
}
