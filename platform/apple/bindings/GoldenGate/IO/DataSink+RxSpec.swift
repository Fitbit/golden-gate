//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSink+RxSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/13/17.
//

import Foundation
@testable import GoldenGate
import Nimble
import Quick
import RxSwift

class DataSinkRxSpec: QuickSpec {
    override func spec() {
        let disposeBag = DisposeBag()

        it("can be bound to observables") {
            waitUntil { done in
                let sentData = "hello".data(using: .utf8)!

                let dataSink = MockDataSink { buffer in
                    expect(buffer.data).to(equal(sentData))
                    done()
                }

                Observable.just(ManagedBuffer(data: sentData))
                    .bind(to: dataSink)
                    .disposed(by: disposeBag)
            }
        }
    }
}
