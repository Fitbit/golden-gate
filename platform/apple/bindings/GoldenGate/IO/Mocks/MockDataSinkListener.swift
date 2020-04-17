//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockDataSinkListener.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//

import Foundation
@testable import GoldenGate
import RxSwift

class MockDataSinkListener: DataSinkListener {
    let onCanPutSubject = PublishSubject<Void>()

    func onCanPut() {
        onCanPutSubject.on(.next(()))
    }
}
