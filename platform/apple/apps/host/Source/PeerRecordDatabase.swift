//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Tracker.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 11/2/17.
//

import Foundation
import GoldenGate
import RxCocoa
import RxSwift

protocol DatabaseContext: class {
    func recordDidChange()
}

class PeerRecordDatabase: DatabaseContext, Codable {
    private let disposeBag = DisposeBag()
    private let recordsRelay = BehaviorRelay(value: [PeerRecord]())
    private(set) var recordByHandle = [PeerRecordHandle: PeerRecord]()
    private var didChangeSubject = PublishSubject<Void>()

    init() {
        monitorChangesToRecords()
    }

    required init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()

        while !container.isAtEnd {
            let record = try container.decode(PeerRecord.self)
            record.context = self
            recordByHandle[record.handle] = record
        }

        recordsRelay.accept(Array(recordByHandle.values))

        monitorChangesToRecords()
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.unkeyedContainer()

        for record in recordsRelay.value {
            try container.encode(record)
        }
    }

    var records: Observable<[PeerRecord]> {
        return recordsRelay.asObservable()
    }

    /// Retrieves a record.
    func get(_ handle: PeerRecordHandle) -> PeerRecord? {
        return recordByHandle[handle]
    }

    /// Creates a new record with a given name.
    func create(name: String) -> PeerRecord {
        let record = PeerRecord(handle: PeerRecordHandle(), name: name)
        insert(record)
        return record
    }

    /// Deletes record from the database.
    func delete(_ record: PeerRecord) {
        if recordByHandle.removeValue(forKey: record.handle) != nil {
            let newRecords = recordsRelay.value.filter { $0.handle != record.handle }
            recordsRelay.accept(newRecords)
        }
    }

    /// Broadcast that a record of the database changed.
    func recordDidChange() {
        didChangeSubject.on(.next(()))
    }

    /// Observable that sends whenever a record of the database changed.
    var didChange: Observable<Void> {
        return didChangeSubject.asObservable()
    }
}

private extension PeerRecordDatabase {
    func monitorChangesToRecords() {
        records.asObservable()
            .skip(1)
            .map { _ in () }
            .subscribe(didChangeSubject.asObserver())
            .disposed(by: disposeBag)
    }

    func insert(_ record: PeerRecord) {
        record.context = self
        recordByHandle[record.handle] = record
        var newRecords = self.recordsRelay.value
        newRecords.append(record)
        recordsRelay.accept(newRecords)
    }
}

protocol PeerRecordDatabaseStore {
    /// Loads the database or creates a new one.
    ///
    /// - Returns: The loadeded or created database.
    func load() -> PeerRecordDatabase

    /// Stores the database.
    ///
    /// - Parameter database: The database to store.
    func store(_ database: PeerRecordDatabase)
}

class PeerRecordDatabaseUserDefaultsStore: PeerRecordDatabaseStore {
    private let userDefaults: UserDefaults
    private let key: String

    init(userDefaults: UserDefaults, key: String) {
        self.userDefaults = userDefaults
        self.key = key
    }

    func load() -> PeerRecordDatabase {
        guard let data = userDefaults.data(forKey: key) else {
            return PeerRecordDatabase()
        }

        let decoder = PropertyListDecoder()

        do {
            return try decoder.decode(PeerRecordDatabase.self, from: data)
        } catch {
            // swiftlint:disable:next no_print
            print("Failed to read database: \(error)")
            userDefaults.removeObject(forKey: key)
        }

        return PeerRecordDatabase()
    }

    func store(_ database: PeerRecordDatabase) {
        let encoder = PropertyListEncoder()

        do {
            let data = try encoder.encode(database)
            userDefaults.set(data, forKey: key)
            userDefaults.synchronize()
        } catch {
            // swiftlint:disable:next no_print
            print("Failed to write database: \(error)")
            return
        }
    }
}

/// A store that does not store anything and always
/// creates a new database when trying to load one.
class VoidPeerRecordDatabaseStore: PeerRecordDatabaseStore {
    func store(_ database: PeerRecordDatabase) {
        // nothing
    }

    func load() -> PeerRecordDatabase {
        return PeerRecordDatabase()
    }
}
