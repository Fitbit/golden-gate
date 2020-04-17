//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapStaticMessageBody.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//

import RxSwift

internal final class CoapStaticMessageBody: CoapMessageBody {
    /// If `nil`, body was already consumed
    internal private(set) var data: Data?

    init(data: Data?) {
        // Always set it so that body can be consumed.
        self.data = data ?? Data()
    }

    func asData() -> Single<Data> {
        guard let data = data else {
            return .error(CoapMessageBodyError.bodyAlreadyUsed)
        }

        return .just(data)
    }

    func asStream() -> Observable<Data> {
        return asData().asObservable()
    }
}
