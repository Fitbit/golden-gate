//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BufferSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/26/18.
//

import Foundation
@testable import GoldenGate
import GoldenGateXP
import Nimble
import Quick

class BufferSpec: QuickSpec {
    override func spec() {
        let data = Data("hello".utf8)

        describe("UnmanagedBuffer") {
            it("bridges to Swift") {
                data.withUnsafeBytes { (bytes: UnsafeRawBufferPointer) in
                    var buffer = GG_StaticBuffer()
                    GG_StaticBuffer_Init(
                        &buffer,
                        bytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        data.count
                    )
                    let unmanaged = UnmanagedBuffer(&buffer)
                    expect(unmanaged.data) == data
                }
            }
        }

        describe("ManagedBuffer") {
            it("can be created with data") {
                expect(ManagedBuffer(data: data).data) == data
            }

            it("can be briged to C and back") {
                let managed = ManagedBuffer(data: data)
                let unmanaged = UnmanagedBuffer(managed.ref)
                expect(unmanaged.data) == data
            }

            it("is deallocated") {
                weak var weakManaged: GoldenGate.ManagedBuffer?

                do {
                    let managed = ManagedBuffer(data: data)
                    weakManaged = managed
                    _ = UnmanagedBuffer(managed.ref)
                }

                expect(weakManaged).to(beNil())
            }
        }
    }
}
