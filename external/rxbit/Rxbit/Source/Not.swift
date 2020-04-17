//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Not.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 9/26/18.
//

import RxSwift

extension ObservableType where Element == Bool {
	/// Inverts each Bool sent by the Observable.
	///
	/// - Returns: an Observable of inverted Bools.
	public func not() -> Observable<Bool> {
		return self.map(!)
	}
}
