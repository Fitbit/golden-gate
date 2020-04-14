//
//  Or.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/15/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import RxCocoa
import RxSwift

public extension ObservableConvertibleType where Element == (Bool, Bool) {
	/// Performs a boolean OR on the tuble of Bool values sent by the Observable.
	///
	/// Note: This allows doing construct such as
	///    Observable.combineLatest(O1, O2)
    ///        .or()
	///        .map { value: Bool in ... }
	///
	/// - Returns: an Observable of Bools.
	func or() -> Observable<Bool> {
		return self.asObservable()
			.map { Mirror(reflecting: $0).children.lazy.compactMap { $0.1 as? Bool } }
			.map { Rxbit.or($0) }

	}
}

public extension ObservableConvertibleType where Element == (Bool, Bool, Bool) {
	func or() -> Observable<Bool> {
		return self.asObservable()
			.map { Mirror(reflecting: $0).children.lazy.compactMap { $0.1 as? Bool } }
			.map { Rxbit.or($0) }

	}
}

private func or(_ values: [Bool]) -> Bool {
	// prefer iterator over `reduce` to exit early
	for value in values {
		if value { return true } // swiftlint:disable:this for_where superfluous_disable_command
	}

	return false
}
