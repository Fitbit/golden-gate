//
//  TlsKeyResolver.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/21/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import GoldenGateXP

public final class TlsKeyResolver {
    private let gg: RunLoopGuardedValue<GGTlsKeyResolver>

    public var ref: UnsafeMutablePointer<GG_TlsKeyResolver> {
        return gg.value.ref
    }

    public init(runLoop: RunLoop, resolve: @escaping (Data) -> Data?) {
        let keyResolver = GGTlsKeyResolver(resolve: resolve)
        self.gg = RunLoopGuardedValue(keyResolver, runLoop: runLoop)
    }

    public convenience init(runLoop: RunLoop, identity: Data, key: Data) {
        self.init(runLoop: runLoop) { requestedIdentity in
            guard identity == requestedIdentity else { return nil }
            return key
        }
    }
}

/// Wrapper around GG_TlsKeyResolverInterface.
final private class GGTlsKeyResolver: GGAdaptable {
    typealias GGObject = GG_TlsKeyResolver
    typealias GGInterface = GG_TlsKeyResolverInterface

    let adapter: Adapter

    let resolve: (Data) -> Data?

    private static var interface = GG_TlsKeyResolverInterface(
        ResolveKey: { resolverRef, identityRef, identitySize, keyRef, keySize in
            .from {
                let maximumKeyLength = keySize!.pointee
                let resolver = Adapter.takeUnretained(resolverRef)
                let identity = UnsafeBufferPointer(start: identityRef, count: identitySize)
                let outKey = UnsafeMutableBufferPointer(start: keyRef, count: maximumKeyLength)

                guard let key = resolver.resolve(Data(identity)) else {
                    throw GGRawError.noSuchItem
                }

                guard key.count <= maximumKeyLength else {
                    throw GGRawError.notEnoughSpace
                }

                keySize!.pointee = key.copyBytes(to: outKey)
            }
        }
    )

    fileprivate init(resolve: @escaping (Data) -> Data?) {
        self.resolve = resolve

        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    fileprivate convenience init(identity: Data, key: Data) {
        self.init { requestedIdentity in
            guard identity == requestedIdentity else { return nil }
            return key
        }
    }

    deinit {
        runLoopPrecondition(condition: .onRunLoop)
    }
}
