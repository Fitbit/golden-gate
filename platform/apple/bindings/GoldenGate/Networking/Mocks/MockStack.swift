//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockStack.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/4/18.
//

@testable import GoldenGate
import RxSwift

class MockStack: StackType {}

class MockStackBuilder: StackBuilderType {
    typealias Stack = MockStack

    func build(link: Observable<NetworkLink?>) -> Observable<MockStack?> {
        return .just(nil)
    }
}
