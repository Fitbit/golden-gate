//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  SocketAddressSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/11/18.
//

import Foundation
import GoldenGateXP
import Nimble
import Quick
import RxSwift

@testable import GoldenGate

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class SocketAddressSpec: QuickSpec {
    func testX() {
        // This is here because otherwise Quick tests don't appear in Test Navigator
    }

    override func spec() {
        describe("Equatable conformance") {
            it("is equal if address and port are the same") {
                let address1 = SocketAddress(address: try! IpAddress(string: "127.0.0.1"), port: 1234)
                let address2 = SocketAddress(address: try! IpAddress(string: "127.0.0.1"), port: 1234)
                expect(address1) == address2
            }

            it("is not equal if port is different") {
                let address1 = SocketAddress(address: try! IpAddress(string: "127.0.0.1"), port: 1234)
                let address2 = SocketAddress(address: try! IpAddress(string: "127.0.0.1"), port: 5678)
                expect(address1) != address2
            }

            it("is not equal if IP is different") {
                let address1 = SocketAddress(address: try! IpAddress(string: "8.8.8.8"), port: 1234)
                let address2 = SocketAddress(address: try! IpAddress(string: "8.8.4.4"), port: 1234)
                expect(address1) != address2
            }
        }

        describe("CustomStringConvertible") {
            it("describes it") {
                let address = SocketAddress(address: try! IpAddress(string: "127.0.0.1"), port: 1234)
                expect(address.description) == "127.0.0.1:1234"
            }
        }

        describe("URL") {
            it("parses IP and port") {
                let address = SocketAddress(url: URL(string: "//127.0.0.1:1234")!)
                expect(address?.description) == "127.0.0.1:1234"
            }

            it("parses protocols") {
                let address = SocketAddress(url: URL(string: "http://127.0.0.1")!)
                expect(address?.description) == "127.0.0.1:80"
            }

            it("parses hostnames") {
                let address = SocketAddress(url: URL(string: "//localhost:42")!)
                expect(address?.description) == "127.0.0.1:42"
            }

            it("returns nil with too little info") {
                expect(SocketAddress(url: URL(string: "hello")!)).to(beNil())
            }

            it("returns nil if no port is provided") {
                expect(SocketAddress(url: URL(string: "//localhost")!)).to(beNil())
            }

            it("returns nil if an unknown protocol is provided") {
                expect(SocketAddress(url: URL(string: "something://localhost")!)).to(beNil())
            }
        }
    }
}
