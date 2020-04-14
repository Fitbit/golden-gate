//
//  DisposeBag+Extensions.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/29/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import Foundation
import RxSwift

/// Adds the right-hand-side disposable to the left-hand-side
/// `DisposeBag`.
///
/// ````
/// disposeBag += observable
///     .filter { ... }
///     .map    { ... }
///     .subscribe(observer)
/// ````
///
/// - parameters:
///   - lhs: DisposeBag to add to.
///   - rhs: Disposable to add.
public func += (lhs: DisposeBag, rhs: Disposable?) {
	guard let rhs = rhs else { return }
	return rhs.disposed(by: lhs)
}

/// Adds the right-hand-side closure to the left-hand-side
/// `DisposeBag`.
///
/// ````
/// disposeBag += { ... }
/// ````
///
/// - parameters:
///   - lhs: DisposeBag to add to.
///   - rhs: Closure to add as a disposable.
public func += (lhs: DisposeBag, rhs: @escaping () -> Void) {
	return lhs.insert(Disposables.create(with: rhs))
}
