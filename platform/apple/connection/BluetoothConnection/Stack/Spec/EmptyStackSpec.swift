//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  EmptyStackSpec.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 22/06/2020.
//

@testable import BluetoothConnection
import Nimble
import Quick
import RxCocoa
import RxSwift
import RxTest

// swiftlint:disable force_try

final class EmptyStackSpec: QuickSpec {
    override func spec() {
        var linkSourceBuffer: DataBuffer!
        var linkSinkBuffer: [Data]!
        var stack: EmptyStack!

        var scheduler: TestScheduler!
        var disposeBag: DisposeBag!

        let someData1 = "SOME_DATA_1".data(using: .utf8)!
        let someData2 = "SOME_DATA_2".data(using: .utf8)!

        beforeEach {
            linkSourceBuffer = DataBuffer(size: 8)
            linkSinkBuffer = []

            stack = EmptyStack(
                link: NetworkLink(
                    dataSink: MockDataSink { linkSinkBuffer.append($0.data) },
                    dataSource: linkSourceBuffer,
                    mtu: BehaviorRelay<UInt>(value: 0)
                )
            )

            scheduler = TestScheduler(initialClock: 0)
            disposeBag = DisposeBag()
        }

        it("forwards data unmodified from the top port to the network link") {
            try! stack.topPort?.dataSink.put(someData1)
            try! stack.topPort?.dataSink.put(someData2)

            expect(linkSinkBuffer) == [someData1, someData2]
        }

        it("forwards data unmodified from the network link to the top port") {
            let observer = scheduler.createObserver(Data.self)

            stack.topPort?.dataSource
                .asObservable()
                .map { $0.data }
                .subscribe(observer)
                .disposed(by: disposeBag)

            try! linkSourceBuffer.put(someData1)
            try! linkSourceBuffer.put(someData2)

            expect(observer.events).to(haveCount(2))
            expect(observer.events[0].value.element) == someData1
            expect(observer.events[1].value.element) == someData2
        }
    }
}
