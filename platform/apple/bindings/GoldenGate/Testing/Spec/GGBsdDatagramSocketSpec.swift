//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGBsdDatagramSocketSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/12/18.
//

import Foundation
import Nimble
import Quick
import RxSwift

@testable import GoldenGate

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class GGBsdDatagramSocketSpec: QuickSpec {
    override func spec() {
        var runLoop: GoldenGate.RunLoop!

        beforeEach {
            runLoop = GoldenGate.RunLoop()
            runLoop.start()
        }

        afterEach {
            runLoop.terminate()
        }

        describe("init") {
            it("can be a client") {
                _ = try! GGBsdDatagramSocket(
                    runLoop: runLoop,
                    localAddress: nil,
                    remoteAddress: SocketAddress(url: URL(string: "//127.0.0.1:34666")!)
                )
            }

            it("can be a server") {
                _ = try! GGBsdDatagramSocket(
                    runLoop: runLoop,
                    localAddress: SocketAddress(url: URL(string: "//127.0.0.1:34666")!),
                    remoteAddress: nil
                )
            }
        }

        // Disabled until `GG_Loop_MonitorFileDescriptors` can handle
        // mutation of `monitor_handlers` while iterating over them.
        // swiftlint:disable:next disabled_quick_test
        xit("communicates with another socket") {
            let address1 = SocketAddress(url: URL(string: "//127.0.0.1:63666")!)
            let address2 = SocketAddress(url: URL(string: "//127.0.0.1:64666")!)

            let socket1 = try! GGBsdDatagramSocket(
                runLoop: runLoop,
                localAddress: address1,
                remoteAddress: address2
            )

            let socket2 = try! GGBsdDatagramSocket(
                runLoop: runLoop,
                localAddress: address2,
                remoteAddress: address1
            )

            let outgoingMessage = Data("hello".utf8)

            waitUntil { done in
                runLoop.async {
                    try! socket2.put(outgoingMessage)
                }

                _ = socket1.asObservable().take(1).subscribe(onNext: { incomingMessage in
                    expect(incomingMessage.data) == outgoingMessage
                    done()
                })
            }
        }
    }
}
