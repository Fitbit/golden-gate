//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGAdapterSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/6/17.
//

@testable import GoldenGate
import GoldenGateXP
import Nimble
import Quick

class GGAdapterSpec: QuickSpec {
    fileprivate static var interface = GG_ExampleInterface(tag: 123)

    override func spec() {
        var example: MyExample!

        beforeEach {
            example = MyExample(answer: 42)
        }

        describe("passUnretained") {
            it("returns the base pointer") {
                let adapter = ExampleAdapter(iface: &GGAdapterSpec.interface).bind(to: example)
                let basePointer = ExampleAdapter.passUnretained(adapter)
                expect(basePointer.pointee.iface.pointee.tag).to(equal(123))
            }

            it("throws if bind was not called before") {
                let adapter = ExampleAdapter(iface: &GGAdapterSpec.interface).bind(to: example)
                let basePointer = ExampleAdapter.passUnretained(adapter)
                _ = adapter.unbind()

                expect { _ = ExampleAdapter.passUnretained(adapter) }.to(throwAssertion())
                expect { _ = ExampleAdapter.takeUnretained(basePointer) }.to(throwAssertion())
            }
        }

        describe("takeUnretained") {
            it("restored the object from a base pointer") {
                let adapter = ExampleAdapter(iface: &GGAdapterSpec.interface).bind(to: example)
                let basePointer = ExampleAdapter.passUnretained(adapter)
                let restored = ExampleAdapter.takeUnretained(basePointer)
                expect(restored).to(be(example))
                expect(restored.answer).to(equal(42))
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

private class MyExample {
    let answer: Int

    init(answer: Int) {
        self.answer = answer
    }
}

private typealias ExampleAdapter = GGAdapter<GG_Example, GG_ExampleInterface, MyExample>
