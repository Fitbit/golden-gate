//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  VoidDataSinkSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//

import Foundation
@testable import GoldenGate
import Nimble
import Quick

class VoidDataSinkSpec: QuickSpec {
    override func spec() {
        it("does nothing") {
            expect({
                try VoidDataSink.instance.put(Data())
                try VoidDataSink.instance.setDataSinkListener(nil)
            }).toNot(throwError())
        }
    }
}
