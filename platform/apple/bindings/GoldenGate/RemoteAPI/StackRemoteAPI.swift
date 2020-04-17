//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  StackRemoteAPI.swift
//  GoldenGate
//
//  Created by Tudor Marinescu on 16/05/2018.
//

import GoldenGateXP
import RxSwift

public typealias RemoteApiPeerId = String

public protocol StackConfigurable: class {
    func setStackDescriptor(_ descriptor: StackDescriptor)
    func setServiceDescriptor(_ descriptor: ServiceDescriptor)
}

public class StackRemoteAPI: RemoteApiModule {
    // The property is weak so that the reference will be set to nil
    // when `globalConfigurable` deallocates.
    public weak var globalConfigurable: StackConfigurable?

    public let methods: Set<String> = Method.allUniqueRawValues

    public init() {}

    public func publishHandlers(on remoteShell: RemoteShell) {
        remoteShell.register(Method.setType.rawValue, handler: setType)
    }

    private func setType(_ params: SetTypeParams) -> Completable {
        return .deferred {
            guard let globalConfigurable = self.globalConfigurable else {
                return .error(Error.stackNotFound)
            }

            globalConfigurable.setStackDescriptor(params.stackType.stackDescriptor)
            if let service = params.service {
                globalConfigurable.setServiceDescriptor(service.serviceDescriptor)
            }

            return .empty()
        }
    }
}

private extension StackRemoteAPI {
    enum Method: String, CaseIterable {
        case setType = "stack/set_type"
    }

    enum Error: Swift.Error {
        case stackNotFound
    }
}

private struct SetTypeParams: Decodable {
    enum StackType: String, Decodable {
        case gattlink
        case udp
        case dtls
        case dtlsa

        var stackDescriptor: StackDescriptor {
            switch self {
            case .gattlink:
                return StackDescriptor.gattlink
            case .udp:
                return StackDescriptor.socketNetifGattlink
            case .dtls:
                return StackDescriptor.dtlsSocketNetifGattlink
            case .dtlsa:
                return StackDescriptor.dtlsSocketNetifGattlinkActivity
            }
        }
    }

    enum Service: String, Decodable {
        case coap
        case blast

        var serviceDescriptor: ServiceDescriptor {
            switch self {
            case .blast:
                return .blasting
            case .coap:
                return .coap
            }
        }
    }

    let stackType: StackType
    let service: Service?
}
