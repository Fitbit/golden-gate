//
//  MockStack.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
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
