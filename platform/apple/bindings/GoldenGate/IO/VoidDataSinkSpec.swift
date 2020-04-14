//
//  VoidDataSinkSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//  Copyright © 2017 Fitbit. All rights reserved.
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
