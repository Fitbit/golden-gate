//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  UITableViewCell+Rx.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/20/17.
//

#if os(iOS)

import Foundation
import RxCocoa
import RxSwift
import UIKit

extension Reactive where Base: UITableViewCell {
    var checked: Binder<Bool> {
        return Binder(self.base) { cell, checked in
            cell.accessoryType = checked ? .checkmark : .none
        }
    }
}

#endif
