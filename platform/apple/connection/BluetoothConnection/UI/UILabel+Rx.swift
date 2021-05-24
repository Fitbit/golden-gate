//
//  UILabel+Rx.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 11/20/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

#if os(iOS)

import Foundation
import RxCocoa
import RxSwift
import UIKit

extension Reactive where Base: UILabel {
    var textColor: Binder<UIColor?> {
        return Binder(self.base) { label, color in
            label.textColor = color
        }
    }
}

#endif
