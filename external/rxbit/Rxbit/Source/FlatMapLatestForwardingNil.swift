//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  FlatMapLatestForwardingNil.swift
//  Rxbit
//
//  Created by Marcel Jackwerth on 11/20/17.
//

import Foundation
import RxSwift

/// Utility functions on observables that contain options where we want a `nil`
/// to just trickle down the observable without acting upon it.
///
/// Useful in cases where you have `invalidation` of values
/// that have been sent on the observable before, like a CoreBluetooth service.
public extension ObservableConvertibleType where Element: OptionalType {
	func flatMapLatestForwardingNil<O: ObservableConvertibleType>(_ selector: @escaping (Element.Wrapped) throws -> O) -> Observable<O.Element?> {
		return asObservable().flatMapLatest { optional -> Observable<O.Element?> in
			try optional
				.map { value in
					try selector(value).asObservable().map { $0 as O.Element? }
				} ?? Observable.just(nil).map { $0 as O.Element? }
		}
	}

	func flatMapLatestForwardingNil<O: ObservableConvertibleType>(_ selector: @escaping (Element.Wrapped) throws -> O) -> Observable<O.Element> where O.Element: OptionalType {
		return asObservable().flatMapLatest { optional -> Observable<O.Element> in
			try optional
				.map { value in
					try selector(value).asObservable()
				} ?? Observable.just(O.Element.none())
		}
	}

	func mapForwardingNil<R>(_ selector: @escaping (Element.Wrapped) throws -> R) -> Observable<R?> {
		return asObservable().map { optional -> R? in
			try optional.map { try selector($0) }
		}
	}
}
