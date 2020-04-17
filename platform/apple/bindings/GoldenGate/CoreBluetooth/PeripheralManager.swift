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
    private let configuration: BluetoothConfiguration
    private var services = [CBUUID: PeripheralManagerService]()

    let scheduler: SchedulerType
    internal let peripheralManager: CBPeripheralManager
    let isAdvertising = BehaviorSubject(value: false)

    var state: Observable<CBManagerState> {
        return stateSubject
            .asObservable()
            .distinctUntilChanged()
    }

    var readyToUpdateSubscribers: Observable<Void> {
        return delegateWrapper.readyToUpdateSubscribers.asObservable()
    }

    var centralDidSubscribeToCharacteristic: Observable<SubscribedCentral> {
        return delegateWrapper.centralDidSubscribeToCharacteristic
            .do(onNext: { (central, characteristic) in
                LogBluetoothInfo("PeripheralManager: Central \(central) did subscribe to \(characteristic)")
            })
            .asObservable()
    }

    var centralDidUnsubscribeFromCharacteristic: Observable<SubscribedCentral> {
        return delegateWrapper.centralDidUnsubscribeFromCharacteristic
            .do(onNext: { (central, characteristic) in
                LogBluetoothInfo("PeripheralManager: Central \(central) did unsubscribe from \(characteristic)")
            })
            .asObservable()
    }

    var didReceiveReadRequest: Observable<CBATTRequest> {
        return delegateWrapper.didReceiveReadRequestCharacteristic.asObservable()
    }

    var didReceiveWriteRequests: Observable<[CBATTRequest]> {
        return delegateWrapper.didReceiveWriteRequestsCharacteristic.asObservable()
    }

    private let stateSubject = BehaviorSubject(value: CBManagerState.unknown)

    let name: BehaviorRelay<String>

    private let disposeBag = DisposeBag()

    // swiftlint:disable:next function_body_length
    public init(queue: DispatchQueue, options: [String: AnyObject]? = nil, configuration: BluetoothConfiguration) {
        self.delegateWrapper = CBPeripheralManagerDelegateWrapper()
        self.scheduler = SerialDispatchQueueScheduler(queue: queue, internalSerialQueueName: queue.label)
        self.peripheralManager = CBPeripheralManager(
            delegate: delegateWrapper,
            queue: queue,
            options: options
        )
        self.queue = queue
        self.configuration = configuration
        self.name = BehaviorRelay(value: configuration.name)

        delegateWrapper.didUpdateState.asObservable()
            .distinctUntilChanged()
            .do(onNext: { [weak self] state in
                LogBluetoothInfo("\(self ??? "PeripheralManager").rx_state: \(state)")
            })
            .bind(to: stateSubject)
            .disposed(by: disposeBag)

        delegateWrapper.didReceiveReadRequestCharacteristic
            .asObservable()
            .do(onNext: { [weak self] request in
                guard let `self` = self else { return }

                guard let service = self.services[request.characteristic.service.uuid] else {
                    self.peripheralManager.respond(to: request, withResult: .readNotPermitted)
                    LogBluetoothError("\(self ??? "PeripheralManager") invalid GATT read request \(request)")
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

                for request in requests {
                    guard let service = self.services[request.characteristic.service.uuid] else {
                        self.peripheralManager.respond(to: request, withResult: .writeNotPermitted)
                        LogBluetoothError("\(self ??? "PeripheralManager") invalid GATT write request \(request)")
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

        LogBluetoothDebug("PeripheralManager init")
    }

    deinit {
        LogBluetoothDebug("PeripheralManager deinit")
    }

    public func publish(service: PeripheralManagerService) -> Completable {
        return Completable
            .create { _ in
                let disposable = CompositeDisposable()
                disposable += self.stateOncePoweredOn()
                    .take(1)
                    .observeOn(self.scheduler)
                    .subscribe(onNext: { _ in
                        guard self.services[service.service.uuid] == nil else { return }

                        LogBluetoothInfo("Adding service... \(service)")
                        self.services[service.service.uuid] = service
                        self.peripheralManager.add(service.service)
                    })

                disposable += Disposables.create {
                    guard self.services[service.service.uuid] != nil else { return }
                    LogBluetoothInfo("Removing service... \(service)")
                    self.services.removeValue(forKey: service.service.uuid)
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

                    let uuids = self.services
                        .values
                        .filter { $0.advertise }
                        .map { $0.service.uuid }

                    let advertisementData: [String: Any] = [
                        CBAdvertisementDataLocalNameKey: name,
                        CBAdvertisementDataServiceUUIDsKey: uuids
                    ]

                    self.isAdvertising.on(.next(true))
                    self.peripheralManager.startAdvertising(advertisementData)

                    return Disposables.create {
                        self.isAdvertising.on(.next(false))

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
            assertLogBluetoothError("Failed to add service: \(error)")
        }

        didAddService.on(.next((service, error)))
    }

    fileprivate let readyToUpdateSubscribers = PublishSubject<Void>()

    public func peripheralManagerIsReady(toUpdateSubscribers peripheral: CBPeripheralManager) {
        readyToUpdateSubscribers.on(.next(()))
    }

    public func peripheralManager(_ peripheral: CBPeripheralManager, willRestoreState dict: [String: Any]) {
        // do nothing
        LogBluetoothInfo("\(self) peripheralmanager state restored for \(dict)")
    }
}

public protocol PeripheralManagerService: class {
    var advertise: Bool { get }
    var service: CBMutableService { get }
    var peripheralManager: PeripheralManager? { get set }
    var didReceiveRead: PublishSubject<ReadRequest> { get }
    var didReceiveWrite: PublishSubject<WriteRequest> { get }
}

extension CBCentral {
    // NOTE: `CBConnection` was added in macOS 10.13 SDK.
    // Now Xcode believes that `identifier` did not exist before...
    //
    // https://forums.developer.apple.com/thread/84375
    var hackyIdentifier: UUID {
        // swiftlint:disable:next force_cast
        return value(forKey: "identifier") as! NSUUID as UUID
    }
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
