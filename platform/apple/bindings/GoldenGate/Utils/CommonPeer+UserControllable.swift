//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CommonPeer+UserControllable.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 8/1/18.
//

import ObjectiveC
import Rxbit
import RxCocoa
import RxSwift

private var subscriberContext: UInt8 = 0

extension CommonPeer {
    public func setUserWantsToConnect(_ connect: Bool) {
        userWantsToConnectSubscriber.accept(connect)
    }

    public var userWantsToConnect: Driver<Bool> {
        return userWantsToConnectSubscriber.subscribed.asDriver(onErrorJustReturn: false)
    }

    private var userWantsToConnectSubscriber: BooleanSubscriber {
        if let value = objc_getAssociatedObject(self, &subscriberContext) {
            // swiftlint:disable:next force_cast
            return value as! BooleanSubscriber
        }

        let value = BooleanSubscriber(
            source: connectionController.establishConnection()
                .logInfo("Attempting to establish connection.", .bindings, .subscribe)
        )
        objc_setAssociatedObject(self, &subscriberContext, value, .OBJC_ASSOCIATION_RETAIN)

        return value
    }
}
