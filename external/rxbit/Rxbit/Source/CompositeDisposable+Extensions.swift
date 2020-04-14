//
//  CompositeDisposable.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 1/10/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation
import RxSwift

/// Adds the right-hand-side disposable to the left-hand-side
/// `CompositeDisposable`.
///
/// ````
/// disposable += observable
///     .filter { ... }
///     .map    { ... }
///     .subscribe(observer)
/// ````
///
/// - parameters:
///   - lhs: Disposable to add to.
///   - rhs: Disposable to add.
///
/// - returns: An instance of `DisposeKey` that can be used to opaquely
///            remove the disposable later (if desired).
@discardableResult
public func += (lhs: CompositeDisposable, rhs: Disposable?) -> CompositeDisposable.DisposeKey? {
	guard let rhs = rhs else { return nil }
	return lhs.insert(rhs)
}

/// Adds the right-hand-side closure to the left-hand-side
/// `CompositeDisposable`.
///
/// ````
/// disposable += { ... }
/// ````
///
/// - parameters:
///   - lhs: Disposable to add to.
///   - rhs: Closure to add as a disposable.
///
/// - returns: An instance of `DisposeKey` that can be used to opaquely
///            remove the disposable later (if desired).
@discardableResult
public func += (lhs: CompositeDisposable, rhs: @escaping () -> Void) -> CompositeDisposable.DisposeKey? {
	return lhs.insert(Disposables.create(with: rhs))
}

/// Removes the right-hand-side disposable from the left-hand-side
/// `CompositeDisposable`.
///
/// ````
/// let disposableKey = disposable += { ... }
/// ...
/// disposable -= disposableKey
/// ````
///
/// - parameters:
///   - lhs: Disposable to add to.
///   - rhs: Closure to add as a disposable.
public func -= (lhs: CompositeDisposable, rhs: CompositeDisposable.DisposeKey?) {
	guard let rhs = rhs else { return }
	lhs.remove(for: rhs)
}
