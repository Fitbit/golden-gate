//
//  DataBuffer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/30/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation

/// Buffers a given amount of buffers before it reports `BluetoothConnectionError.wouldBlock`
/// back to its source. Useful if a source does not react on `BluetoothConnectionError.wouldBlock`
public class DataBuffer: DataSource, DataSink, DataSinkListener {
    private let size: Int
    private var queue = [(buffer: Buffer, metadata: DataSink.Metadata?)]()

    private var canPut = true
    private var reportedFailure = false
    private var dataSink: DataSink?
    private weak var dataSinkListener: DataSinkListener?

    /// - Parameter size: The number of buffers to buffer before reporting
    ///   `BluetoothConnectionError` to the source.
    public init(size: Int) {
        assert(size > 0)
        self.size = size
    }

    public convenience init(size: Int, dataSink: DataSink) throws {
        self.init(size: size)
        try self.setDataSink(dataSink)
    }

    private var hasSpace: Bool {
        return queue.count < size
    }

    public func put(_ buffer: Buffer, metadata: Metadata?) throws {
        guard !reportedFailure && hasSpace else {
            reportedFailure = true
            LogBluetoothWarning("DataBuffer: WOULD_BLOCK")
            throw BluetoothConnectionError.wouldBlock
        }

        queue.append((buffer: buffer, metadata: metadata))
        try flush()
    }

    public func onCanPut() {
        canPut = true
        _ = try? flush()
    }

    public func setDataSink(_ dataSink: DataSink?) throws {
        self.dataSink = dataSink
        try dataSink?.setDataSinkListener(self)
        onCanPut()
    }

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        self.dataSinkListener = dataSinkListener
    }

    private func flush() throws {
        while canPut, let entry = queue.first, let dataSink = dataSink {
            do {
                try dataSink.put(entry.buffer, metadata: entry.metadata)
                queue.removeFirst()
            } catch BluetoothConnectionError.wouldBlock {
                canPut = false
                break
            }
        }

        notify()
    }

    private func notify() {
        guard hasSpace && reportedFailure else { return }
        LogBluetoothDebug("DataBuffer: notify")
        reportedFailure = false
        dataSinkListener?.onCanPut()
    }
}
