//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeripheralManager.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/27/17.
//

import CoreBluetooth
import Foundation
import Rxbit
import RxCocoa
import RxSwift

enum PeripheralManagerError: Error {
    case destroyed
    case unsubscribed
}

public enum CentralSubscriptionStatus {
    case unsubscribed
    case subscribed(CBCentral)
}

public extension CentralSubscriptionStatus {
    var central: CBCentral? {
        switch self {
        case .unsubscribed: return nil
        case .subscribed(let central): return central
        }
    }
}

public typealias SubscribedCentral = (central: CBCentral, characteristic: CBCharacteristic)

public class PeripheralManager {
    private let delegateWrapper: CBPeripheralManagerDelegateWrapper
    private let queue: DispatchQueue
    private var publishedServices = [CBUUID: PeripheralManagerService]()
    private var pendingServices = [CBUUID: PeripheralManagerService]()
    private let isAdvertisingSubject = BehaviorSubject(value: false)

    public let scheduler: SchedulerType
    public let peripheralManager: CBPeripheralManager

    public var isAdvertising: Observable<Bool> {
        return isAdvertisingSubject.asObservable()
    }

    public var state: Observable<CBManagerState> {
        return stateSubject
            .asObservable()
            .distinctUntilChanged()
    }

    public var readyToUpdateSubscribers: Observable<Void> {
        return delegateWrapper.readyToUpdateSubscribers.asObservable()
    }

    public var centralDidSubscribeToCharacteristic: Observable<SubscribedCentral> {
        return delegateWrapper.centralDidSubscribeToCharacteristic
            .do(onNext: { (central, characteristic) in
                LogBluetoothInfo("PeripheralManager: Central \(central) did subscribe to \(characteristic)")
            })
            .asObservable()
    }

    public var centralDidUnsubscribeFromCharacteristic: Observable<SubscribedCentral> {
        return delegateWrapper.centralDidUnsubscribeFromCharacteristic
            .do(onNext: { (central, characteristic) in
                LogBluetoothInfo("PeripheralManager: Central \(central) did unsubscribe from \(characteristic)")
            })
            .asObservable()
    }

    public var didReceiveReadRequest: Observable<CBATTRequest> {
        return delegateWrapper.didReceiveReadRequestCharacteristic.asObservable()
    }

    public var didReceiveWriteRequests: Observable<[CBATTRequest]> {
        return delegateWrapper.didReceiveWriteRequestsCharacteristic.asObservable()
    }

    public let name: BehaviorRelay<String>

    private let stateSubject = BehaviorSubject(value: CBManagerState.unknown)
    
    private let disposeBag = DisposeBag()

    // swiftlint:disable:next function_body_length
    public init(queue: DispatchQueue, options: [String: AnyObject]? = nil, name: String) {
        self.delegateWrapper = CBPeripheralManagerDelegateWrapper()
        self.scheduler = SerialDispatchQueueScheduler(queue: queue, internalSerialQueueName: queue.label)
        self.peripheralManager = CBPeripheralManager(
            delegate: delegateWrapper,
            queue: queue,
            options: options
        )
        self.queue = queue
        self.name = BehaviorRelay(value: name)

        delegateWrapper.didUpdateState.asObservable()
            .distinctUntilChanged()
            .do(onNext: { state in
                LogBluetoothInfo("PeripheralManager.rx_state: \(state)")
            })
            .bind(to: stateSubject)
            .disposed(by: disposeBag)

        delegateWrapper.didReceiveReadRequestCharacteristic
            .asObservable()
            .do(onNext: { [weak self] request in
                guard let `self` = self else { return }
                LogBluetoothDebug("PeripheralManager: Did receive read request \(request)")

                let cbService: CBService? = request.characteristic.service

                guard let serviceUuid = cbService?.uuid, let service = self.publishedServices[serviceUuid] else {
                    self.peripheralManager.respond(to: request, withResult: .readNotPermitted)
                    LogBluetoothError("PeripheralManager: Invalid GATT read request \(request)")
                    return
                }

                service.didReceiveRead.on(.next(ReadRequest(
                    peripheralManager: self.peripheralManager,
                    request: request
                )))
            })
            .subscribe()
            .disposed(by: disposeBag)

        delegateWrapper.didReceiveWriteRequestsCharacteristic
            .asObservable()
            .do(onNext: { [weak self] requests in
                guard let `self` = self else { return }
                LogBluetoothDebug("PeripheralManager: Did receive write requests \(requests)")

                for request in requests {
                    let cbService: CBService? = request.characteristic.service

                    guard let serviceUuid = cbService?.uuid, let service = self.publishedServices[serviceUuid] else {
                        self.peripheralManager.respond(to: request, withResult: .writeNotPermitted)
                        LogBluetoothError("PeripheralManager: Invalid GATT write request \(request)")
                        return
                    }

                    service.didReceiveWrite.on(.next(WriteRequest(
                        peripheralManager: self.peripheralManager,
                        request: request
                    )))
                }
            })
            .subscribe()
            .disposed(by: disposeBag)

        delegateWrapper
            .didAddService
            .asObservable()
            .do(onNext: { [weak self] service, error in
                guard let self = self else { return }
                LogBluetoothInfo("PeripheralManager: Did add service \(service) with error \(String(describing: error)).")

                if error == nil {
                    if let peripheralManagerService = self.pendingServices[service.uuid] {
                        self.publishedServices[peripheralManagerService.service.uuid] = peripheralManagerService
                    } else if let mutableService = service as? CBMutableService {
                        // If publish(service:) was disposed before didAddService was executed we should remove the service because it's not needed anymore
                        self.peripheralManager.remove(mutableService)
                    }
                }
                self.pendingServices.removeValue(forKey: service.uuid)
            })
            .subscribe()
            .disposed(by: disposeBag)

        LogBluetoothInfo("PeripheralManager: Init")
    }

    deinit {
        LogBluetoothInfo("PeripheralManager: Deinit")
    }

    public func publish(service: PeripheralManagerService) -> Completable {
        return Completable
            .create { _ in
                let disposable = CompositeDisposable()
                disposable += self.stateOncePoweredOn()
                    .take(1)
                    .observeOn(self.scheduler)
                    .subscribe(onNext: { _ in
                        guard
                            self.publishedServices[service.service.uuid] == nil,
                            self.pendingServices[service.service.uuid] == nil
                        else { return }

                        LogBluetoothInfo("PeripheralManager: Trying to add service... \(service)")
                        self.pendingServices[service.service.uuid] = service
                        self.peripheralManager.add(service.service)
                    })

                disposable += Disposables.create {
                    self.pendingServices.removeValue(forKey: service.service.uuid)
                    guard self.publishedServices[service.service.uuid] != nil else { return }
                    LogBluetoothInfo("PeripheralManager: Trying to remove service... \(service)")
                    self.publishedServices.removeValue(forKey: service.service.uuid)
                    self.peripheralManager.remove(service.service)
                }

                return disposable
            }
            .subscribeOn(scheduler)
    }

    private lazy var advertiser: Completable = {
        return Observable.combineLatest(stateOncePoweredOn(), name.distinctUntilChanged())
            .flatMapLatest { [weak self] (state, name) -> Observable<Never> in
                guard state == .poweredOn else { return Observable.never() }

                return .create { observer in
                    guard let `self` = self else {
                        observer.on(.error(PeripheralManagerError.destroyed))
                        return Disposables.create()
                    }

                    let uuids = self.publishedServices
                        .values
                        .filter { $0.advertise }
                        .map { $0.service.uuid }

                    let advertisementData: [String: Any] = [
                        CBAdvertisementDataLocalNameKey: name,
                        CBAdvertisementDataServiceUUIDsKey: uuids
                    ]

                    self.isAdvertisingSubject.on(.next(true))
                    self.peripheralManager.startAdvertising(advertisementData)

                    return Disposables.create {
                        self.isAdvertisingSubject.on(.next(false))

                        if self.peripheralManager.state == .poweredOn {
                            self.peripheralManager.stopAdvertising()
                        }
                    }
                }
            }
            .share(replay: 1)
            .asCompletable()
    }()

    public func advertise() -> Completable {
        return advertiser
    }

    public func stateOncePoweredOn() -> Observable<CBManagerState> {
        return stateSubject
            .skipWhile { $0 != .poweredOn }
            .distinctUntilChanged()
    }
}

private class CBPeripheralManagerDelegateWrapper: NSObject, CBPeripheralManagerDelegate {
    fileprivate let centralDidSubscribeToCharacteristic = PublishSubject<SubscribedCentral>()

    public func peripheralManager(_ peripheral: CBPeripheralManager, central: CBCentral, didSubscribeTo characteristic: CBCharacteristic) {
        centralDidSubscribeToCharacteristic.on(.next((central: central, characteristic: characteristic)))
    }

    fileprivate let centralDidUnsubscribeFromCharacteristic = PublishSubject<SubscribedCentral>()

    public func peripheralManager(_ peripheral: CBPeripheralManager, central: CBCentral, didUnsubscribeFrom characteristic: CBCharacteristic) {
        centralDidUnsubscribeFromCharacteristic.on(.next((central: central, characteristic: characteristic)))
    }

    fileprivate let didUpdateState = ReplaySubject<CBManagerState>.create(bufferSize: 1)

    public func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
        didUpdateState.on(.next(CBManagerState(rawValue: peripheral.state.rawValue)!))
    }

    fileprivate let didReceiveReadRequestCharacteristic = PublishSubject<CBATTRequest>()

    public func peripheralManager(_ peripheral: CBPeripheralManager, didReceiveRead request: CBATTRequest) {
        didReceiveReadRequestCharacteristic.on(.next(request))
    }

    fileprivate let didReceiveWriteRequestsCharacteristic = PublishSubject<[CBATTRequest]>()

    public func peripheralManager(_ peripheral: CBPeripheralManager, didReceiveWrite requests: [CBATTRequest]) {
        didReceiveWriteRequestsCharacteristic.on(.next(requests))
    }

    fileprivate let didAddService = PublishSubject<(CBService, Error?)>()

    public func peripheralManager(_ peripheral: CBPeripheralManager, didAdd service: CBService, error: Error?) {
        if let error = error {
            assertLogBluetoothError("PeripheralManager: Failed to add service: \(error)")
        }

        didAddService.on(.next((service, error)))
    }

    fileprivate let readyToUpdateSubscribers = PublishSubject<Void>()

    public func peripheralManagerIsReady(toUpdateSubscribers peripheral: CBPeripheralManager) {
        readyToUpdateSubscribers.on(.next(()))
    }

    public func peripheralManager(_ peripheral: CBPeripheralManager, willRestoreState dict: [String: Any]) {
        // do nothing
        LogBluetoothInfo("PeripheralManager: state restored for \(dict)")
    }
}

public protocol PeripheralManagerService: AnyObject {
    var advertise: Bool { get }
    var service: CBMutableService { get }
    var peripheralManager: PeripheralManager? { get set }
    var didReceiveRead: PublishSubject<ReadRequest> { get }
    var didReceiveWrite: PublishSubject<WriteRequest> { get }
}

extension CBManagerState: CustomStringConvertible {
    public var description: String {
        switch self {
        case .unknown:
            return "unknown"
        case .resetting:
            return "resetting"
        case .unsupported:
            return "unsupported"
        case .unauthorized:
            return "unauthorized"
        case .poweredOff:
            return "poweredOff"
        case .poweredOn:
            return "poweredOn"
        @unknown default:
            return "unknown"
        }
    }
}
