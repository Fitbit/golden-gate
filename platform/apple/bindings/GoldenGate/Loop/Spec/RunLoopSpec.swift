//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RunLoopSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/6/17.
//

import Foundation
import Nimble
import Quick

@testable import GoldenGate

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length identifier_name

private enum SpecError: Error {
    case test
}

class RunLoopSpec: QuickSpec {
    override func spec() {
        var runLoop: GoldenGate.RunLoop!

        beforeEach {
            runLoop = GoldenGate.RunLoop()
            runLoop.start()
        }

        afterEach {
            runLoop.terminate()
        }

        describe("async") {
            it("executes tasks asynchronously") {
                waitUntil { done in
                    runLoop.async(execute: done)
                }
            }

            it("releases the closure on the run loop") {
                waitUntil { done in
                    // Testing assumes main is a valid run loop,
                    // so let's switch to a background queue.
                    DispatchQueue.global().async {
                        var o: RunLoopObject? = RunLoopObject()
                        runLoop.async { [o] in
                            _ = o
                            done()
                        }
                        o = nil
                    }
                }
            }
        }

        describe("sync") {
            it("executes tasks synchronously") {
                let result: Int = runLoop.sync {
                    runLoopPrecondition(condition: .onRunLoop)
                    return 42
                }

                expect(result).to(equal(42))
            }

            it("reports errors") {
                expect {
                    try runLoop.sync {
                        throw SpecError.test
                    }
                }.to(throwError(SpecError.test))
            }

            it("does not deadlock when run from run loop") {
                var correct = false

                DispatchQueue.global().async {
                    correct = runLoop.sync {
                        runLoop.sync {
                            return true
                        }
                    }
                }

                expect(correct).toEventually(beTrue())
            }

            // Pending: For `sync` we need to use `actuallyNotEscaping`
            // which will release the reference to the block
            pending("releases captured variables on the run loop") {
                waitUntil { done in
                    // Testing assumes main is a valid run loop,
                    // so let's switch to a background queue.
                    DispatchQueue.global().async {
                        do {
                            let o = RunLoopObject()
                            runLoop.sync { [o] in
                                _ = o
                            }
                        }

                        done()
                    }
                }
            }
        }

        describe("asyncAfter") {
            it("executes tasks with a delay") {
                waitUntil { done in
                    runLoop.asyncAfter(deadline: .now() + 0.05) {
                        done()
                    }
                }
            }
        }

        describe("terminate") {
            var notStartedRunLoop: GoldenGate.RunLoop!

            beforeEach {
                notStartedRunLoop = GoldenGate.RunLoop()
            }

            it("will make run() return") {
                waitUntil { done in
                    DispatchQueue.global().async {
                        notStartedRunLoop.terminate()
                    }

                    notStartedRunLoop.run() // this will block
                    done()
                }
            }
        }

        it("deinits values on RunLoop") {
            waitUntil { done in
                // Testing assumes main is a valid run loop,
                // so let's switch to a background queue.
                DispatchQueue.global().async {
                    do {
                        _ = RunLoopGuardedValue(RunLoopObject(), runLoop: runLoop)
                    }

                    done()
                }
            }
        }
    }
}

private class RunLoopObject {
    init() {
        runLoopPrecondition(condition: .notOnRunLoop)
    }

    deinit {
        runLoopPrecondition(condition: .onRunLoop)
    }
}
