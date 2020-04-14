//
//  CoapMessageBuilder.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 02/04/2018.
//  Copyright © 2018 Fitbit, Inc. All rights reserved.
//

public protocol CoapMessageBuilder {
    associatedtype Message

    /// Adds an option.
    ///
    /// - Parameters:
    ///   - number: The number of the option.
    ///   - value: The value of the option.
    /// - Returns: The current message builder.
    func option(number: CoapOption.Number, value: CoapOptionValue) -> Self

    /// Add body.
    ///
    /// - Parameters:
    ///   - data: The data that will be added to the body of the request.
    ///   - progress: Block that will get called each time the progress value changes.
    ///               The value passed as parameter is a value between 0.0 and 1.0.
    /// - Returns: The current message builder.
    func body(data: Data, progress: ((Double) -> Void)?) -> Self

    /// Creates a message with all the properties set.
    ///
    /// - Returns: The message that was created.
    /// - Throws: If one of the properties is not set correctly, the build method will throw.
    func build() throws -> Message
}

extension CoapMessageBuilder {
    /// Appends a string option.
    ///
    /// - Parameters:
    ///   - number: The number of the option.
    ///   - value: The value of the option.
    /// - Returns: The current message builder.
    func option(number: CoapOption.Number, value: String) -> Self {
        return option(number: number, value: .string(value))
    }

    /// Appends a Data option.
    ///
    /// - Parameters:
    ///   - number: The number of the option.
    ///   - value: The value of the option.
    /// - Returns: The current message builder.
    func option(number: CoapOption.Number, value: Data) -> Self {
        return option(number: number, value: .opaque(value))
    }

    /// Appends an uint32 option.
    ///
    /// - Parameters:
    ///   - number: The number of the option.
    ///   - value: The value of the option.
    /// - Returns: The current message builder.
    func option(number: CoapOption.Number, value: UInt32) -> Self {
        return option(number: number, value: .uint(value))
    }

    /// Appends multiple options.
    ///
    /// - Parameter options: The opptions to append.
    /// - Returns: The current message builder.
    func append(options: [CoapOption]) -> Self {
        return options.reduce(self) { builder, option in
            builder.option(number: option.number, value: option.value)
        }
    }
}
