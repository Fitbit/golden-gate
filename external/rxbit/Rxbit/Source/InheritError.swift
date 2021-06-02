//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  InheritError.swift
//  Rxbit
//
//  Created by Marcel Jackwerth on 10/30/17.
//

import Foundation
import RxCocoa
import RxSwift

public extension ObservableType {
	/// Inherit all errors from the second observable. If first observable completes, the returned observable will complete.
	///
	/// - Parameter second: The observable where the errors will be inherited.
	/// - Returns: Observable that emits all the values as the first observable and will also throw error from second observable.
	func inheritError<O: ObservableConvertibleType>(from second: O) -> Observable<Element> {
		let secondWithoutElements = second
			.asObservable()
			.filter { _ in false }
			.map { _ in () }
			.startWith(())

		let work = self.materialize()

		return Observable
			.combineLatest(work, secondWithoutElements)
			.map { $0.0 }
			.take(while: { event in
				switch event {
				case .completed: return false
				default: return true
				}
			})
			.dematerialize()
	}

	func filterError(_ predicate: @escaping (Error) -> Bool) -> Observable<Element> {
		return self.catch { error in
			guard !predicate(error) else { throw error }
			return .empty()
		}
	}
}
