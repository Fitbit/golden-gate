//
//  ViewReusing+Rx.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/6/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

#if os(macOS)

import Cocoa
import RxCocoa
import RxSwift

extension Reactive where Base == NSView {
	/// Utility signal to know when to stop modifying a view.
	var detached: Observable<Void> {
		return Observable
			.amb([
				methodInvoked(#selector(NSView.prepareForReuse)).map { _ in () },
				deallocating
			])
	}
}

extension Reactive where Base == NSTableCellView {
	/// Utility signal to know when to stop modifying a cell.
	var detached: Observable<Void> {
		return (base as NSView).rx.detached
	}
}

#endif
