//
//  And.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/15/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import RxCocoa
import RxSwift

public extension ObservableConvertibleType where Element == (Bool, Bool) {
	/// Performs a boolean AND on the tuple of Bool values sent by the Observable.
	///
	/// This allows doing construct such as:
	/// ````
	/// Observable.combineLatest(O1, O2)
	///     .and()
	///     .map { value: Bool in ... }
	/// ````
	///
	/// - Returns: an Observable of Bools
	func and() -> Observable<Bool> {
		return self.asObservable()
			.map { Mirror(reflecting: $0).children.lazy.compactMap { $0.1 as? Bool } }
			.map { Rxbit.and($0) }

	}
}

public extension ObservableConvertibleType where Element == (Bool, Bool, Bool) {
	func and() -> Observable<Bool> {
		return self.asObservable()
			.map { Mirror(reflecting: $0).children.lazy.compactMap { $0.1 as? Bool } }
			.map { Rxbit.and($0) }

	}
}

public extension ObservableConvertibleType where Element == [Bool] {
	/// Performs a boolean AND on the array of Bool values sent by the Observable.
	///
	/// This allows doing constructs such as:
	/// ````
	/// Observable.combineLatest([O1, O2, ..., On])
	///     .and()
	///     .map { value: Bool in ... }
	/// ````
	///
	/// - Returns: an Observable of Bools
	func and() -> Observable<Bool> {
		return self.asObservable()
			.map { Rxbit.and($0) }
	}
}

private func and(_ values: [Bool]) -> Bool {
	// `allSatisfy` exits early
	return values.allSatisfy { $0 }
}
