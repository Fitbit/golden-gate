//
//  RemoteShellHandler.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/6/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import FbSmo
import GoldenGateXP
import RxSwift

public typealias RequestHandler = (Data?) throws -> Maybe<Data>

final class RemoteShellHandler: GGAdaptable {
    typealias GGObject = GG_RemoteCborHandler
    typealias GGInterface = GG_RemoteCborHandlerInterface

    public let callback: RequestHandler
    let adapter: Adapter
    private let timeout: DispatchTimeInterval
    private let disposeBag = DisposeBag()

    private static var interface = GG_RemoteCborHandlerInterface(
        HandleRequest: { ref, _, rawParams, errorCodeRef, responseRef in
            do {
                let `self` = Adapter.takeUnretained(ref)
                let params = rawParams != nil ? UnmanagedBuffer(rawParams).data : nil
                let maybe = try self.callback(params)
                let response = try self.await(maybe)

                if let response = response {
                    responseRef!.pointee = ManagedBuffer(data: response).passRetained()
                }

                return GG_SUCCESS
            } catch let error as GGRawError {
                return error.ggResult
            } catch {
                let errorData = ErrorData(
                    message: error.localizedDescription,
                    fullError: "\(error)"
                )

                let encoder = FbSmoEncoder()
                encoder.keyEncodingStrategy = .convertToSnakeCase

                let data: Data
                do {
                    data = try encoder.encode(errorData, using: .cbor)
                } catch {
                    return GG_FAILURE
                }

                responseRef!.pointee = ManagedBuffer(data: data).passRetained()

                // Ensure code doesn't fall into the reserved error code range.
                let code = max((error as NSError).code, Int(GG_JSON_RPC_ERROR_GENERIC_SERVER_ERROR + 1))
                errorCodeRef!.pointee = Int16(clamping: code)

                return GG_FAILURE
            }
        }
    )

    init(callback: @escaping RequestHandler, timeout: DispatchTimeInterval = .seconds(10)) {
        self.callback = callback
        self.timeout = timeout

        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    private func await(_ maybe: Maybe<Data>) throws -> Data? {
        var result: Result?
        let semaphore = DispatchSemaphore(value: 0)

        let disposable = maybe
            .subscribe(
                onSuccess: { value in
                    result = .value(value)
                    semaphore.signal()
                },
                onError: { error in
                    result = .error(error)
                    semaphore.signal()
                },
                onCompleted: {
                    semaphore.signal()
                }
            )

        defer { disposable.dispose() }

        guard semaphore.wait(timeout: .now() + timeout) == .success else {
            throw GGRawError.timeout
        }

        switch result {
        case .value(let value)?:
            return value
        case .error(let error as RemoteShellError)?:
            switch error {
            case .invalidParameters:
                throw GGRawError.invalidParameters
            }
        case .error(let error)?:
            throw error
        case nil:
            return nil
        }
    }
}

private enum Result {
    case value(Data)
    case error(Error)
}

private struct ErrorData: Encodable {
    let message: String?
    let fullError: String?
}
