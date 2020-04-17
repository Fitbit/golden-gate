//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapRequestBuilder.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/29/18.
//

import Foundation

/// A builder to create new instances of `CoapRequest`.
///
/// - Note: This class follows the builder pattern.
public class CoapRequestBuilder: CoapMessageBuilder {
    private var path = [String]()
    private var method = CoapCode.Request.get
    private var options = [CoapOption]()
    private var confirmable = true
    private var expectsSuccess = true
    private var acceptsBlockwiseTransfer = true
    private var body = CoapOutgoingBody.none
    private var parameters: GGCoapRequestParameters?

    /// Creates a new instance of the builder.
    public init() {
        // Make it public
    }

    /// Sets the path of the resource to request.
    @discardableResult
    public func path(_ path: [String]) -> Self {
        self.path = path
        return self
    }

    /// Sets the method of the request.
    @discardableResult
    public func method(_ method: CoapCode.Request) -> Self {
        self.method = method
        return self
    }

    /// Sets whether the request should await an ACK.
    @discardableResult
    public func confirmable(_ confirmable: Bool) -> Self {
        self.confirmable = confirmable
        return self
    }

    /// Allows custom handling of 4.xx and 5.xx responses.
    ///
    /// Changes the behavior of the `endpoint.response(request:)` API.
    ///
    /// When set to `true` (default), a 4.xx or 5.xx response will
    /// error the `response` observable.
    /// When set to `false`, a 4.xx or 5.xx response will
    /// be emitted like a 2.xx response.
    @discardableResult
    public func expectsSuccess(_ expectsSuccess: Bool) -> Self {
        self.expectsSuccess = expectsSuccess
        return self
    }

    /// Whether to anticipate block-wise responses (default=true).
    @discardableResult
    public func acceptsBlockwiseTransfer(_ acceptsBlockwiseTransfer: Bool) -> Self {
        self.acceptsBlockwiseTransfer = acceptsBlockwiseTransfer
        return self
    }

    /// Appends an option to the option list.
    ///
    /// - Warning: Use `path()` to add `uriPath` options.
    @discardableResult
    public func option(number: CoapOption.Number, value: CoapOptionValue) -> Self {
        assert(number != .uriPath, "Use path() to set a the uriPath")
        options.append(CoapOption(number: number, value: value))
        return self
    }

    /// Sets the body to a static `Data` buffer and a progress block that
    /// will be called whenever the progress value changes.
    @discardableResult
    public func body(data: Data, progress: ((Double) -> Void)? = nil) -> Self {
        self.body = .data(data, progress)
        return self
    }

    @discardableResult
    public func parameters(ackTimeout: UInt32, resendCount: Int) -> Self {
        self.parameters = GGCoapRequestParameters(ackTimeout: ackTimeout, resendCount: resendCount)
        return self
    }

    /// Builds the `CoapRequest` instance.
    public func build() -> CoapRequest {
        let uriPathOptions = path.map { CoapOption(number: .uriPath, value: .string($0)) }

        return CoapRequest(
            method: method,
            options: uriPathOptions + options,
            confirmable: confirmable,
            expectsSuccess: expectsSuccess,
            acceptsBlockwiseTransfer: acceptsBlockwiseTransfer,
            body: body,
            parameters: parameters
        )
    }
}
