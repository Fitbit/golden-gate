//
//  RunLoop+Rx.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/29/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import RxSwift

// MARK: - Offers the possibility of scheduling units of work on the run loop thread.
extension RunLoop: ImmediateSchedulerType {
    public func schedule<StateType>(_ state: StateType, action: @escaping (StateType) -> Disposable) -> Disposable {
        let disposable = SingleAssignmentDisposable()

        async {
            if disposable.isDisposed {
                return
            }

            disposable.setDisposable(action(state))
        }

        return disposable
    }
}
