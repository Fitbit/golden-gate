//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NSObject+DisposeBag.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 8/30/18.
//

import Foundation
import RxCocoa
import RxSwift

public extension NSObject {
	private static var disposeBagKey: UInt8 = 0

	var disposeBag: DisposeBag {
		get {
			return memoize(&NSObject.disposeBagKey) {
				DisposeBag()
			}
		}
		set {
			synchronize {
				objc_setAssociatedObject(self, &NSObject.disposeBagKey, newValue, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
			}
		}
	}
}
