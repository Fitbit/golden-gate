//
//  Optionalize.swift
//  Rxbit
//
//  Created by Marcel Jackwerth on 11/20/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import RxCocoa
import RxSwift

public extension ObservableConvertibleType {
	func optionalize() -> Observable<Element?> {
		return asObservable().map { .some($0) }
	}
}

public extension SharedSequenceConvertibleType {
	func optionalize() -> SharedSequence<SharingStrategy, Element?> {
		return map { .some($0) }
	}
}
