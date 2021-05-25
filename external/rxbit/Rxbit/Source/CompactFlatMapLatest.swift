//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CompactFlatMapLatest.swift
//  Rxbit
//
//  Created by Emanuel Jarnea on 21/07/2020.
//

import Foundation
import RxSwift

/// Utility functions on observables that contain options where we want a `nil`
/// to be filtered out without trickling down the observable.
public extension ObservableConvertibleType where Element: OptionalType {
	func compactFlatMapLatest<O: ObservableConvertibleType>(_ selector: @escaping (Element.Wrapped) throws -> O) -> Observable<O.Element.Wrapped> where O.Element: OptionalType {
		return compactFlatMapLatest(selector).filterNil()
	}

	func compactFlatMapLatest<O: ObservableConvertibleType>(_ selector: @escaping (Element.Wrapped) throws -> O) -> Observable<O.Element> {
		return asObservable().flatMapLatest { optional -> Observable<O.Element> in
			try optional
				.map { value in
					try selector(value).asObservable()
				} ?? Observable.empty()
		}
	}
}
