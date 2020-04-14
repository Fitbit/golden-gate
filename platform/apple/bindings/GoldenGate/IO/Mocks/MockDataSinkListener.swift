//
//  MockDataSinkListener.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//  Copyright © 2017 Fitbit. All rights reserved.
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
