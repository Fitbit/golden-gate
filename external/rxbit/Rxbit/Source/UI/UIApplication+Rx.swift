//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  UIApplication+Rx.swift
//  Rxbit
//
//  Created by Ryan LaSante on 8/27/18.
//

#if os(iOS) || os(tvOS)

import RxCocoa
import RxSwift
import UIKit

/// Protocol that exposes application state observables.
public protocol UIApplicationStateObservableProviderType {
	/// Starts with the current application state and will update as the application state changes.
	var applicationState: Observable<UIApplication.State> { get }

	/// Emits true when the application state is active and false otherwise. Starts
	/// with a value based on the current application state.
	var applicationStateActive: Observable<Bool> { get }

	/// Emits when the application state becomes active.
	var applicationBecameActive: Observable<Void> { get }
}

public class UIApplicationStateObservableProvider<StateProvider>: UIApplicationStateObservableProviderType
	where StateProvider: ReactiveCompatible & UIApplicationStateProviderType {

	private let provider: StateProvider

	public var applicationState: Observable<UIApplication.State> {
		return provider.rx.applicationState
	}

	public var applicationStateActive: Observable<Bool> {
		return provider.rx.applicationStateActive
	}

	public var applicationBecameActive: Observable<Void> {
		return provider.rx.applicationBecameActive
	}

	public init(provider: StateProvider) {
		self.provider = provider
	}
}

/// Protocol that provides application state.
///
/// Note that it should use Notification.Name.UIApplicationWill/Did
/// notifications upon changing values.
public protocol UIApplicationStateProviderType: AnyObject {
	var applicationState: UIApplication.State { get }
}

extension UIApplication: UIApplicationStateProviderType {}

/// Adds `applicationState` property to Reactive extension so one can do observing
/// of the current UIApplication state
public extension Reactive where Base: UIApplicationStateProviderType {
	/// Starts with the current application state and will update as the application state changes
	/// This is a continuous stream that will only complete with the deallocation of UIApplication.
	///
	/// Note that this will cause subscriptions and fire events on the MainScheduler.
	var applicationState: Observable<UIApplication.State> {
		let notificationObservables = [
			UIApplication.didFinishLaunchingNotification,
			UIApplication.willEnterForegroundNotification,
			UIApplication.didBecomeActiveNotification,
			UIApplication.willResignActiveNotification,
			UIApplication.didEnterBackgroundNotification,
			UIApplication.willTerminateNotification
			]
			.map { NotificationCenter.default.rx.notification($0) }

		return Observable
			.deferred { [unowned base] in
				Observable
					.merge(notificationObservables)
					.map { [unowned base] _ in
						base.applicationState
					}
					.startWith(base.applicationState)
			}
			.distinctUntilChanged()
			.subscribeOn(MainScheduler.instance)
			.takeUntil(deallocated)
	}

	/// Emits true when the application state is active and false otherwise. Starts
	/// with a value based on the current application state.
	var applicationStateActive: Observable<Bool> {
		return applicationState.map { $0 == .active }
	}

	/// Emits when the application state becomes active.
	var applicationBecameActive: Observable<Void> {
		return applicationState.skip(1)
			.filter { $0 == .active }
			.map { _ in  () }
	}
}

extension UIApplication.State: CustomStringConvertible {
	public var description: String {
		switch self {
		case .active: return "active"
		case .background: return "background"
		case .inactive: return "inactive"
		@unknown default: return "unknown"
		}
	}
}

#endif
