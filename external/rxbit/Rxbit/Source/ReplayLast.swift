//
//  ReplayLast.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 10/15/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

import RxSwift

extension ObservableType {
	/// Returns a connected observable sequence that shares a single subscription to the underlying
	/// sequence replaying last element.
	///
	/// - Returns: A connected observable sequence that shares the last element sent by the underlying sequence.
	public func replayLast() -> Observable<Element> {
		let connectable = self.replay(1)
		_ = connectable.connect()

		return connectable.asObservable()
	}
}

extension ObservableConvertibleType {
	public func replayLast() -> Observable<Element> {
		return asObservable().replayLast()
	}
}
