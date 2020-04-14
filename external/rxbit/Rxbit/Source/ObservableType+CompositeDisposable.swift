//
//  ObservableType+CompositeDisposable.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/25/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import RxSwift

extension ObservableType {
	/// Creates an observable sequence from a specified subscribe method implementation.
    ///
	/// Removes the need to always create an empty disposable for non cancellabled observables
	/// or provides one by default if needed.
	///
	/// ````
	/// .create { observer, _ in
	///    observer.on(())
	///    observer.on(.completed)
	/// }
	///
	/// .create { observer, disposable in
	///    ...
	///    disposable += { ... }
	///    disposable += observable.subscribe(observer)
	/// }
	/// ````
	///
	///
	/// seealso: [create operator on reactivex.io](http://reactivex.io/documentation/operators/create.html)
    ///
	/// - Parameter subscribe: Implementation of the resulting observable sequence's `subscribe` method.
	/// - Returns: The observable sequence with the specified implementation for the `subscribe` method.
	public static func create(_ subscribe: @escaping (AnyObserver<Element>, CompositeDisposable) -> Void) -> Observable<Element> {
		return Observable<Element>.create { observer -> Disposable in
			let disposable = CompositeDisposable()
			subscribe(observer, disposable)
			return disposable
		}
	}
}
