//
//  BooleanSubscriber.swift
//  Rxbit
//
//  Created by Marcel Jackwerth on 8/9/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import RxCocoa
import RxSwift

/// Utility class to subscribe to an Observable,
/// depending on a Boolean flag.
public struct BooleanSubscriber {
	public let shouldSubscribe: Observable<Bool>
	public let subscribed: Observable<Bool>

	private let shouldSubscribeRelay: BehaviorRelay<Bool>
	private let disposeBag = DisposeBag()

	public init(source: Completable, subscribe: Bool = false) {
		self.shouldSubscribeRelay = BehaviorRelay<Bool>(value: subscribe)
		self.shouldSubscribe = self.shouldSubscribeRelay.asObservable()

		let connectableSubscribed = shouldSubscribe
			// Don't unsubscribe from a subscription to source,
			// as that might have unwanted/expensive side-effects.
			.flatMapFirst { [shouldSubscribe] subscribe -> Observable<Bool> in
				guard subscribe else {
					return Observable.just(false)
				}

				return source
					.andThen(Observable<Bool>.empty())
					// `flatMapFirst` will ignore any value
					// while the inner observable did not complete
					// so this makes sure that it completes
					// and then reports `false` like the `guard` above.
					.takeUntil(shouldSubscribe.filter { !$0 })
					// When source fails/completes, fall back to `false`.
					.concat(Observable.just(false))
					.catchErrorJustReturn(false)
					.startWith(true)
			}
			.distinctUntilChanged()
			.replay(1)

		self.subscribed = connectableSubscribed.asObservable()
		connectableSubscribed.connect().disposed(by: disposeBag)
	}

	public func accept(_ subscribe: Bool) {
		shouldSubscribeRelay.accept(subscribe)
	}
}
