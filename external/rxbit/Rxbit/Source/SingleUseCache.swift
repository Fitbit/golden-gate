//
//  SingleUseCacheSubject.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import RxSwift

public enum SingleUseCacheSubjectError: Error {
	/// Error reported by `asSignal()` if an observer already
	/// subscribed to the cache.
	case alreadyUsed
}

/// A subject-like object that caches a list of emitted elements until it
/// is consumed by an observer.
///
/// Subscribing to it a second time will fail immediately.
final public class SingleUseCacheSubject<Element> {
	// `indirect` needed as older Swift versions (Xcode 9.2) fail with
	// GenericCache(0x1059b3860): cyclic metadata dependency detected, aborting
	private indirect enum State {
		/// Neither an element has been cached,
		/// nor has an observer obtained access.
		case pristine

		/// An observer has obtained access and
		/// he waits the end of stream.
		case waitingForCompletedEvent(AnyObserver<Element>)

		/// No observer has obtained access yet.
		case waitingForObserver([Event<Element>])

		/// A value has been consumed.
		case resolved
	}

	private let lock = NSRecursiveLock()
	private var state = State.pristine

	public init () {}

	/// Emits the given event to the first observer
	/// that subscribes to the observable returned by `asObservable()` or
	/// appends the given element to the list returned by 'asSingle()'.
	public func on(_ event: Event<Element>) {
		lock.lock()
		defer {
			lock.unlock()
		}

		switch state {
		case .pristine:
			state = .waitingForObserver([event])
		case .waitingForObserver(let events):
			var newEvents = events
			newEvents.append(event)
			state = .waitingForObserver(newEvents)
		case let .waitingForCompletedEvent(observer):
			observer.on(event)
		case .resolved:
			break // ignore
		}
	}

	/// An Observable emitting cached and future elements.
	///
	/// If someone else subscribed to this signal, it will error immediately.
	public func asObservable() -> Observable<Element> {
		return Observable<Element>.create { observer in
			self.lock.lock()
			defer {
				self.lock.unlock()
			}
			switch self.state {
			case .pristine:
				self.state = .waitingForCompletedEvent(observer)
			case .waitingForObserver(let events):
				self.state = .waitingForCompletedEvent(observer)
				events.forEach { observer.on($0) }
			case .waitingForCompletedEvent, .resolved:
				observer.onError(SingleUseCacheSubjectError.alreadyUsed)
			}

			return Disposables.create {
				// Release the value eagerly
				self.lock.lock()
				self.state = .resolved
				self.lock.unlock()
			}
		}
	}

	/// A Single emitting a list of all emitted elements.
	///
	/// If someone else subscribed to this signal, it will error immediately.
	public func asSingle() -> Single<[Element]> {
		return asObservable()
			.scan([Element](), accumulator: { list, element in
				var newList = list
				newList.append(element)

				return newList
			})
			.startWith([Element]())
			.takeLast(1)
			.asSingle()
	}
}
