//
//  DataSink+Rx.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//  Copyright © 2017 Fitbit. All rights reserved.
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
