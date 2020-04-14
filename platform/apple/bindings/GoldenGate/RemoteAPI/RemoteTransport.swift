//
//  RemoteTransport.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/6/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

// swiftlint:disable no_print

import GoldenGateXP
import RxSwift

public enum TransportLayerError: Error {
    case disconnected
}

public protocol TransportLayer: class {
    /// Observable that emits received data.
    var didReceiveData: Observable<Data> { get }

    /// Send data to the transport layer.
    ///
    /// - Parameter data: The data that will be sent.
    func send(data: Data) throws
}

public final class RemoteTransport: GGAdaptable {
    typealias GGObject = GG_RemoteTransport
    typealias GGInterface = GG_RemoteTransportInterface

    private var transportLayer: TransportLayer
    private let semaphore = DispatchSemaphore(value: 0)
    private let queue = DispatchQueue(label: "remote_transport")
    private var dataQueue: [Data] = []
    private let disposeBag = DisposeBag()

    let adapter: Adapter

    private static var interface = GG_RemoteTransportInterface(
        Send: { ref, bufferRef in
            .from {
                let data = UnmanagedBuffer(bufferRef).data
                try Adapter.takeUnretained(ref).send(data: data)
            }
        },
        Receive: { ref, bufferRef in
            let data = Adapter.takeUnretained(ref).receive()
            bufferRef?.pointee = ManagedBuffer(data: data).passRetained()
            return GG_SUCCESS
        }
    )

    init(transportLayer: TransportLayer) {
        self.transportLayer = transportLayer

        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    deinit {
        runLoopPrecondition(condition: .onRunLoop)
    }

    func connect() {
        transportLayer.didReceiveData
            .subscribe(onNext: { [weak self] data in
                self?.didReceiveData(data)
            })
            .disposed(by: disposeBag)
    }

    func send(data: Data) throws {
        try transportLayer.send(data: data)
    }

    func receive() -> Data {
        // We block here until we receive data from the transport layer.
        print("Waiting to receive something...")
        semaphore.wait()

        print("Data received!")
        return queue.sync {
            dataQueue.removeFirst()
        }
    }

    private func didReceiveData(_ data: Data) {
        queue.sync {
            dataQueue.append(data)
        }

        semaphore.signal()
        print("Transport layer reports that new data has arrived! \(data as NSData)")
    }
}
