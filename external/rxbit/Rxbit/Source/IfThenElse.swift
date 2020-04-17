//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  IfThenElse.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/29/18.
//

import Foundation
import RxSwift

public extension ObservableType {
	/// Switches between `trueObservable` and `falseObservable` based on the latest value
	/// sent by `boolObservable`.
	///
	/// - Parameters:
	///   - boolObservable: An observable of Bools determining whether `trueObservable` or
	///                     `falseObservable` should be active.
	///   - trueObservable: The observable to pass through after `boolObservable` has sent true.
	///   - falseObservable: The observable to pass through after `boolObservable` has sent false.
	/// - Returns: An observable which passes through `next`s and `error`s from `trueObservable`
	/// and/or `falseObservable`, and sends `completed` when both `boolObservable` and the
	/// last switched observable complete.
	static func `if`(_ boolObservable: Observable<Bool>, then trueObservable: Observable<Element>, else falseObservable: Observable<Element>) -> Observable<Element> {
		return boolObservable.flatMapLatest {
			$0 ? trueObservable : falseObservable
		}
	}
}
