//
//  ExampleSpec.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 11/27/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Nimble
import Quick

class ExampleSpec: QuickSpec {

    override func spec() {
        let answer = 42

        describe("Quick and Nimble") {
            it("know the answer") {
                expect(answer).to(equal(42))
            }
        }
    }

}
