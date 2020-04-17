//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGAdaptableSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//

import Foundation
@testable import GoldenGate
import Nimble
import Quick

class GGAdaptableSpec: QuickSpec {
    override func spec() {
        describe("ref") {
            it("returns the base pointer") {
                let adaptable = ExampleAdaptable(tag: 42)
                expect(adaptable.ref.pointee.iface.pointee.tag).to(equal(42))
            }
        }

        describe("from") {
            it("returns the main object from a base pointer") {
                let adaptable = ExampleAdaptable(tag: 42)
                expect(ExampleAdaptable.Adapter.takeUnretained(adaptable.ref)).to(be(adaptable))
            }
        }
    }
}

// swiftlint:disable:next type_name
private struct GG_Example {
    let iface: UnsafeMutablePointer<GG_ExampleInterface>
}

// swiftlint:disable:next type_name
private struct GG_ExampleInterface {
    let tag: Int
}

private final class ExampleAdaptable: GGAdaptable {
    typealias GGObject = GG_Example
    typealias GGInterface = GG_ExampleInterface

    let adapter: Adapter

    let interfacePointer: UnsafeMutablePointer<GG_ExampleInterface>

    init(tag: Int) {
        interfacePointer = UnsafeMutablePointer<GG_ExampleInterface>.allocate(capacity: 1)
        interfacePointer.pointee = GG_ExampleInterface(tag: tag)
        self.adapter = Adapter(iface: interfacePointer)
        self.adapter.bind(to: self)
    }

    deinit {
        interfacePointer.deallocate()
    }
}
