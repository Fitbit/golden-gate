//
//  FilterNil.swift
//  Rxbit
//
//  Created by Sylvain Rebaud on 1/11/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation
import RxSwift

public extension ObservableConvertibleType where Element: OptionalType {
	func filterNil() -> Observable<Element.Wrapped> {
		return asObservable().flatMap { optional -> Observable<Element.Wrapped> in
			Observable<Element.Wrapped>.from(optional: optional.asOptional())
		}
	}

	func filterNilKeepOptional() -> Observable<Element> {
		return asObservable().filter { $0.asOptional() != nil }
	}
}
