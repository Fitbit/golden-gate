//
//  Peripheral_GG.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/30/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import Rxbit
import RxBluetoothKit
import RxSwift

public enum PeripheralError: Error {
    case serviceNotFound
    case servicesChangedDuringDiscovery
    case servicesInvalidated
    case invalidProtocolDetected
}

public extension ObservableType where Element == [Service]? {
    /// Returns the service that has the UUID passed as parameter.
    /// If the service does not exist in the list emitted by the observable a "serviceNotFound" error will be thrown.
    ///
    /// - Parameter serviceUUID: The UUID of the service that is required.
    /// - Returns: Observable that emits the required service or nil.
    func requiredServiceForwardingNil(_ serviceUUID: CBUUID) -> Observable<Service?> {
        return self.mapForwardingNil { services -> Service in
            guard let service = services.first(where: { $0.uuid == serviceUUID })
            else { throw PeripheralError.serviceNotFound }

            return service
        }
    }
}

public extension ObservableType where Element == [Service] {
    /// Returns the service that has the UUID passed as parameter.
    /// If the service does not exist in the list emitted by the observable a "serviceNotFound" error will be thrown.
    ///
    /// - Parameter serviceUUID: The UUID of the service that is required.
    /// - Returns: Observable that emits the required service.
    func requiredService(_ serviceUUID: CBUUID) -> Observable<Service> {
        return self.map { services -> Service in
            guard let service = services.first(where: { $0.uuid == serviceUUID })
            else { throw PeripheralError.serviceNotFound }
            
            return service
        }
    }
}

extension Peripheral {
    /// Returns a list of discovered services. The Observable will reemit the list if the GATT database changes.
    ///
    /// - Parameters:
    ///   - serviceUUIDs: A list of service UUIDs that needs to be discovered.
    ///   - blacklistedServiceUUIDMatcher: Peripherals advertising services that match this
    ///             should not appear in the list returned by the service discovery process.
    ///             If the service appears, an PeripheralError.invalidProtocolDetected will be thrown.
    /// - Returns: Observable that emits the list of services. It never completes.
    public func validServices(_ serviceUUIDs: [CBUUID], blacklistedServiceUUIDMatcher: ((CBUUID) -> Bool)? = nil) -> Observable<[Service]> {
        return Single
            // Workaround: RxBluetoothKit might have returned `.just`,
            // `deferred` forces it to re-check the cache.
            .deferred {
                return self.discoverServices(nil)
            }
            // Monitor service change indication to trigger rediscovery via `serviceChanged` error
            .asObservable()
            .takeUntil(
                self.observeServicesModification()
                    .map { _ -> [Service] in throw PeripheralError.servicesChangedDuringDiscovery }
            )
            .do(onNext: { discoveredServices in
                LogBluetoothInfo("Discovered Peripheral Services \(discoveredServices.map { $0.uuid }) for request with services \(serviceUUIDs)")
            })
            // Ensure the service is in that list
            .map { services in
                let blacklistedService = services.first(where: { service in
                    if let blacklistedServiceUUIDMatcher = blacklistedServiceUUIDMatcher {
                        return blacklistedServiceUUIDMatcher(service.uuid)
                    } else {
                        return false
                    }
                })

                guard blacklistedService == nil else { throw PeripheralError.invalidProtocolDetected }

                return services.filter { serviceUUIDs.contains($0.uuid) }
            }
            .flatMap { services -> Observable<[Service]> in
                // Monitor service invalidation
                self.observeServicesModification()
                    // Filter out the services that are not of interest.
                    .filter { _, services in
                        let modifiedServiceUUIDs = Set<CBUUID>(services.map { $0.uuid })
                        let monitoredServiceUUIDs = Set<CBUUID>(serviceUUIDs)

                        return !modifiedServiceUUIDs.union(monitoredServiceUUIDs).isEmpty
                    }
                    .map { _ -> [Service] in throw PeripheralError.servicesInvalidated }
                    .startWith(services)
            }
            .do(onSubscribe: { LogBluetoothInfo("Discovering Peripheral Services \(serviceUUIDs)...") })
            .do(onNext: { discoveredServices in
                LogBluetoothInfo("Filtered Peripheral Services \(discoveredServices.map { $0.uuid }) for request with services \(serviceUUIDs)")
            })
            .retryWhen { errors in
                errors.map { error -> Void in
                    switch error {
                    case PeripheralError.servicesChangedDuringDiscovery:
                        LogBluetoothWarning("Services Change during discovery of services \(serviceUUIDs).")
                        return ()
                    case PeripheralError.servicesInvalidated:
                        LogBluetoothWarning("Services \(serviceUUIDs) were invalidated after discovery.")
                        return ()
                    default:
                        throw error
                    }
                }
            }
    }
}

extension Peripheral: CustomStringConvertible {
    public var description: String {
        return "\(type(of: self)) \(peripheral.name ??? "nil") (\(peripheral.hackyIdentifier))"
    }
}

public extension PeripheralConnectionStatus {
    var peripheral: Peripheral? {
        switch self {
        case .connected(let peripheral): return peripheral
        case .disconnected, .connecting: return nil
        }
    }
}
