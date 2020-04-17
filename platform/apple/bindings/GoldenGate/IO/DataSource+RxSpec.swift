//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSource+RxSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/13/17.
//

import Foundation
@testable import GoldenGate
import Nimble
import Quick
import RxSwift

// swiftlint:disable force_try

class DataSourceRxSpec: QuickSpec {
    override func spec() {
        let disposeBag = DisposeBag()

        it("can be bound to observables") {
            waitUntil { done in
                let sentData = "hello".data(using: .utf8)!
                let dataSource = MockDataSource()

                dataSource.asObservable()
                    .subscribe(onNext: { buffer in
                        expect(buffer.data).to(equal(sentData))
                        done()
                    })
                    .disposed(by: disposeBag)

                try! dataSource.dataSink?.put(sentData)
            }
        }
    }
}

