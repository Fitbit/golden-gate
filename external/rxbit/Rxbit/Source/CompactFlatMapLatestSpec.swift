//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CompactFlatMapLatestSpec.swift
//  Rxbit
//
//  Created by Emanuel Jarnea on 21/07/2020.
//

import Foundation
import Nimble
import Quick
import RxSwift

class CompactFlatMapLatestSpec: QuickSpec {
	override func spec() {
		var disposeBag: DisposeBag!

		beforeEach {
			disposeBag = DisposeBag()
		}

		describe("with a map function with a non-optional result") {
			it("filters nil") {
				let values = Observable<Int?>.from([10, nil, 20])
				let interval = Observable<Int64>.interval(.milliseconds(50), scheduler: MainScheduler.asyncInstance).take(3)
				let observable = Observable.zip(values, interval).map(\.0)

				waitUntil { done in
					observable
						.compactFlatMapLatest { value in
							Observable.from([0, 1, 2].map { $0 + value })
						}
						.toArray()
						.subscribe(onSuccess: { array in
							expect(array).to(equal([10, 11, 12, 20, 21, 22]))
							done()
						})
						.disposed(by: disposeBag)
				}
			}
		}

		describe("with a map function with an optional result") {
			it("filters nil") {
				let values = Observable<Int?>.from([10, nil, 20])
				let interval = Observable<Int64>.interval(.milliseconds(50), scheduler: MainScheduler.asyncInstance).take(3)
				let observable = Observable.zip(values, interval).map(\.0)

				waitUntil { done in
					observable
						.compactFlatMapLatest { value in
							Observable.from([0, nil, 2].map { $0.map { $0 + value } })
						}
						.toArray()
						.subscribe(onSuccess: { array in
							expect(array).to(equal([10, 12, 20, 22]))
							done()
						})
						.disposed(by: disposeBag)
				}
			}
		}
	}
}
