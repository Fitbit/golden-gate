//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSource+Rx.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//

import Foundation
import RxSwift

public extension DataSource {
    func asObservable() -> Observable<Buffer> {
        return Observable.using({ RxDataSink() },
            observableFactory: { sink in
                try self.setDataSink(sink)
                return sink.subject
            }
        )
    }
}

private class RxDataSink: DataSink, Disposable {
    let subject = ReplaySubject<Buffer>.create(bufferSize: 1)

    func put(_ buffer: Buffer, metadata: Metadata?) throws {
        subject.on(.next(buffer))
    }

    func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        // ignore
    }

    func dispose() {
        // ignore
    }
}
