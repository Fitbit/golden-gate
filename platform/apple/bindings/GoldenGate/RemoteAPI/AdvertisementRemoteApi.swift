//
//  AdvertisementRemoteApi.swift
//  GoldenGateHost-macOS
//
//  Created by Bogdan Vlad on 05/03/2018.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGateXP
import RxCocoa
import RxSwift

public class AdvertisementRemoteApi: RemoteApiModule {
    private let advertiser: Node.Advertiser
    private let shouldAdvertise = BehaviorRelay(value: false)
    private let disposeBag = DisposeBag()

    public let methods: Set<String> = Method.allUniqueRawValues

    public init(advertiser: Node.Advertiser) {
        self.advertiser = advertiser

        shouldAdvertise
            .distinctUntilChanged()
            .flatMapLatest { [advertiser] shouldAdvertise -> Completable in
                if shouldAdvertise {
                    return advertiser.advertise()
                } else {
                    return .never()
                }
            }
            .subscribe()
            .disposed(by: disposeBag)
    }

    public func publishHandlers(on remoteShell: RemoteShell) {
        remoteShell.register(Method.setAdvertisedName.rawValue, handler: setAdvertisedName)
        remoteShell.register(Method.advertise.rawValue, handler: advertise)
    }

    private func setAdvertisedName(_ params: SetAdvertisedNameParams) {
        self.advertiser.name.accept(params.name)
    }

    private func advertise() {
        self.shouldAdvertise.accept(true)
    }
}

private extension AdvertisementRemoteApi {
    enum Method: String, CaseIterable {
        case setAdvertisedName  = "bt/set_advertised_name"
        case advertise          = "advertisement/advertise"
    }
}

private struct SetAdvertisedNameParams: Decodable {
    let name: String
}
