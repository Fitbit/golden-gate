//
//  DataSource+RxSpec.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/13/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

@testable import BluetoothConnection
import Foundation
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

