//
//  Not.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 9/26/18.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
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
