//
//  Data+NetworkExtensionsSpec.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

@testable import BluetoothConnection
import Nimble
import Quick

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class DataNetworkExtensionsSpec: QuickSpec {
    override func spec() {
        var data: Data!
        beforeEach {
            data = Data()
        }

        describe("append(littleEndian:)") {
            it("handles UInt8") {
                let value: UInt8 = 0x12
                data.append(littleEndian: value)

                expect(data.count) == 1
                expect([UInt8](data)[0]) == value
                expect(data.asLittleEndian()) == value
            }

            it("handles UInt16") {
                let value: UInt16 = 0x1234
                data.append(littleEndian: value)

                expect(data.count) == 2
                expect(data.asLittleEndian() as UInt16) == value
            }

            it("handles UInt32") {
                let value: UInt32 = 0x12345678
                data.append(littleEndian: value)

                expect(data.count) == 4
                expect(data.asLittleEndian() as UInt32) == value
            }
        }

        describe("append(bigEndian:)") {
            it("handles UInt8") {
                let value: UInt8 = 0x12
                data.append(bigEndian: value)

                expect(data.count) == 1
                expect([UInt8](data)[0]) == value
                expect(data.asBigEndian()) == value
            }

            it("handles UInt16") {
                let value: UInt16 = 0x1234
                data.append(bigEndian: value)

                expect(data.count) == 2
                expect(data.asBigEndian() as UInt16) == value
            }

            it("handles UInt32") {
                let value: UInt32 = 0x12345678
                data.append(bigEndian: value)

                expect(data.count) == 4
                expect(data.asBigEndian() as UInt32) == value
            }
        }

        describe("asBigEndian") {
            it("asserts when passing incorrect number of bytes") {
                let value: UInt32 = 0x12345678
                data.append(bigEndian: value)

                expect { _ = data[0...0].asBigEndian() as UInt16 }.to(throwAssertion())
                expect { _ = data[0...0].asBigEndian() as UInt32 }.to(throwAssertion())
                expect { _ = data[0...1].asBigEndian() as UInt32 }.to(throwAssertion())
                expect { _ = data[0...2].asBigEndian() as UInt32 }.to(throwAssertion())
            }
        }

        describe("asLittleEndian") {
            it("asserts when passing incorrect number of bytes") {
                let value: UInt32 = 0x12345678
                data.append(bigEndian: value)

                expect { _ = data[0...0].asLittleEndian() as UInt16 }.to(throwAssertion())
                expect { _ = data[0...0].asLittleEndian() as UInt32 }.to(throwAssertion())
                expect { _ = data[0...1].asLittleEndian() as UInt32 }.to(throwAssertion())
                expect { _ = data[0...2].asLittleEndian() as UInt32 }.to(throwAssertion())
            }
        }

        describe("toLittleEndian") {
            it("handles UInt8") {
                let value: UInt8 = 0x12
                data.append(littleEndian: value)
                expect(data) == value.toLittleEndian()
            }

            it("handles UInt16") {
                let value: UInt16 = 0x1234
                data.append(littleEndian: value)
                expect(data) == value.toLittleEndian()
            }

            it("handles UInt32") {
                let value: UInt32 = 0x12345678
                data.append(littleEndian: value)
                expect(data) == value.toLittleEndian()
            }
        }

        describe("toBigEndian") {
            it("handles UInt8") {
                let value: UInt8 = 0x12
                data.append(bigEndian: value)
                expect(data) == value.toBigEndian()
            }

            it("handles UInt16") {
                let value: UInt16 = 0x1234
                data.append(bigEndian: value)
                expect(data) == value.toBigEndian()
            }

            it("handles UInt32") {
                let value: UInt32 = 0x12345678
                data.append(bigEndian: value)
                expect(data) == value.toBigEndian()
            }
        }
    }
}
