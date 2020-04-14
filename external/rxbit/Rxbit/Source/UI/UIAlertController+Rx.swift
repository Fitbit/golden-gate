//
//  UIAlertController+Rx.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 7/12/19.
//  Copyright © 2019 Fitbit, Inc. All rights reserved.
//

#if os(iOS) || os(tvOS)

import RxCocoa
import RxSwift
import UIKit

public protocol AlertActionType {
	associatedtype Result

	var title: String? { get }
	var style: UIAlertAction.Style { get }
	var result: Result { get }
}

public struct AlertAction<R>: AlertActionType {
	public typealias Result = R

	public let title: String?
	public let style: UIAlertAction.Style
	public let result: R

	public static func action(title: String?, style: UIAlertAction.Style = .default, result: Result) -> AlertAction<Result> {
		return AlertAction<Result>(title: title, style: style, result: result)
	}
}

public typealias DefaultAlertAction = AlertAction<UIAlertController.Result>

public extension UIAlertController {
	enum Result {
		case affirmative
		case other
	}

	static func present<Action: AlertActionType, Result>(
		in viewController: UIViewController,
		title: String?,
		message: String?,
		style: UIAlertController.Style,
		actions: [Action]
	) -> Single<Result> where Action.Result == Result {
		let alertController = UIAlertController(title: title, message: message, preferredStyle: style)
		return alertController.rx.present(in: viewController, style: style, actions: actions)
	}
}

public extension Reactive where Base: UIAlertController {
	func present<Action: AlertActionType, Result>(
		in viewController: UIViewController,
		style: UIAlertController.Style,
		actions: [Action]
	) -> Single<Result> where Action.Result == Result {
		return Single.create { [unowned base] observer in
			actions.enumerated().forEach { _, action in
				let action = UIAlertAction(title: action.title, style: action.style) { _ in
					observer(.success(action.result))
				}
				base.addAction(action)
			}

			viewController.present(base, animated: true, completion: nil)
			return Disposables.create { base.dismiss(animated: true, completion: nil) }
		}
	}
}

#endif
