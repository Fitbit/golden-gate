//
//  CoapGroupRequestFilter.swift
//  GoldenGate
//
//  Created by Vlad-Mihai Corneci on 06/06/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import GoldenGateXP
import RxSwift

/// Provides an API to setup/update the CoAP request filters.
class CoapGroupRequestFilter {
    typealias Ref = OpaquePointer?

    internal let ref: Ref
    private let runLoop: RunLoop

    init(_ runLoop: RunLoop) throws {
        runLoopPrecondition(condition: .onRunLoop)
        self.runLoop = runLoop

        var ref: OpaquePointer?
        try GG_CoapGroupRequestFilter_Create(&ref).rethrow()
        self.ref = ref!
        GG_CoapGroupRequestFilter_SetGroup(ref, CoapRequestFilterGroup.group1.rawValue).check()
    }

    func setGroup(_ group: CoapRequestFilterGroup) -> Completable {
        return Completable.create { [ref, runLoop] completable in
            runLoop.async {
                do {
                    try GG_CoapGroupRequestFilter_SetGroup(ref, group.rawValue).rethrow()
                    completable(.completed)
                } catch {
                    completable(.error(error))
                }
            }

            return Disposables.create()
        }
    }

    deinit {
        runLoop.async { [ref] in
            GG_CoapGroupRequestFilter_Destroy(ref)
        }
    }
}

/// Group request filters
///   group #0 (i.e. no filtering) is used for authenticated connections
///   group #1 is used for non-authenticated connections
public enum CoapRequestFilterGroup: UInt8 {
    case group0 = 0, group1
}
