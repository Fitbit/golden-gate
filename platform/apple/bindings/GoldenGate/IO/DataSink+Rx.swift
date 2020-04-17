//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSink+Rx.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//

import Foundation
import RxSwift

extension DataSink {
    func asObserver() -> AnyObserver<Buffer> {
        return AnyObserver(eventHandler: on)
    }

    private func on(_ event: Event<Buffer>) {
        guard case .next(let element) = event else { return }
        _ = try? put(element)
    }
}

extension ObservableConvertibleType where Element == Buffer {
    func bind(to dataSink: DataSink) -> Disposable {
        return asObservable().subscribe(dataSink.asObserver())
    }
}
