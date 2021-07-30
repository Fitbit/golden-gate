//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapStaticBlockSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/1/18.
//

import Foundation
import GoldenGateXP

final class CoapStaticBlockSource: GGAdaptable, UnsafeCoapBlockSource {

    // MARK: - GGAdaptable

    typealias GGObject = GG_CoapBlockSource
    typealias GGInterface = GG_CoapBlockSourceInterface
    let adapter: Adapter

    private static var interface = GG_CoapBlockSourceInterface(
        GetDataSize: { ref, offset, sizeRef, moreRef in
            .from {
                let `self` = Adapter.takeUnretained(ref)
                let (size, more) = try self.getDataSize(offset: offset, size: sizeRef!.pointee)
                sizeRef!.pointee = size
                moreRef?.pointee = more
            }
        },
        GetData: { ref, offset, size, outRef in
            .from {
                try Adapter.takeUnretained(ref).getData(
                    offset: offset,
                    size: size,
                    destination: outRef!.assumingMemoryBound(to: UInt8.self)
                )
            }
        }
    )

    // MARK: - Properties

    private let data: Data
    private let progress: ((Double) -> Void)?

    // MARK: - Constructors

    init(data: Data, progress: ((Double) -> Void)? = nil) {
        self.data = data
        self.progress = progress

        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    // MARK: - Methods

    func getDataSize(offset: Int, size: Int) throws -> (size: Int, more: Bool) {
        var size = size
        var more = true
        try GG_CoapMessageBlockInfo_AdjustAndGetChunkSize(offset, &size, &more, data.count).rethrow()
        return (size: size, more: more)
    }

    func getData(offset: Int, size: Int, destination: UnsafeMutablePointer<UInt8>) throws {
        assert(offset < data.count)
        assert(offset + size <= data.count)
        progress?(Double(offset + size) / Double(data.count))

        let rangeStart = data.startIndex.advanced(by: offset)
        let rangeEnd = rangeStart.advanced(by: size)
        let range = Range(uncheckedBounds: (rangeStart, rangeEnd))

        data.copyBytes(to: destination, from: range)
    }
}
