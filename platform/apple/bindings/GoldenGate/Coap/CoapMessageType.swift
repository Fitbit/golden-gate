//
//  CoapMessageType.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGateXP

public enum CoapMessageType {
    /// Some messages require an acknowledgement.  These messages are
    /// called "Confirmable". When no packets are lost, each Confirmable
    /// message elicits exactly one return message of type Acknowledgement
    /// or type Reset.
    case confirmable

    /// Some other messages do not require an acknowledgement. This is
    /// particularly true for messages that are repeated regularly for
    /// application requirements, such as repeated readings from a sensor.
    case nonConfirmable

    /// An Acknowledgement message acknowledges that a specific
    /// Confirmable message arrived.
    case acknowledgement

    /// A Reset message indicates that a specific message (Confirmable or
    /// Non-confirmable) was received, but some context is missing to
    /// properly process it.  This condition is usually caused when the
    /// receiving node has rebooted and has forgotten some state that
    /// would be required to interpret the message.  Provoking a Reset
    /// message (e.g., by sending an Empty Confirmable message) is also
    /// useful as an inexpensive check of the liveness of an endpoint
    /// ("CoAP ping").
    case reset
}

/// Convert between Swift and GG XP CoAP message type.
extension CoapMessageType {
    init(value: GG_CoapMessageType) {
        switch value {
        case GG_COAP_MESSAGE_TYPE_CON:
            self = .confirmable
        case GG_COAP_MESSAGE_TYPE_NON:
            self = .nonConfirmable
        case GG_COAP_MESSAGE_TYPE_ACK:
            self = .acknowledgement
        case GG_COAP_MESSAGE_TYPE_RST:
            self = .reset
        default:
            preconditionFailure("Wrong CoAP Message type. \(value)")
        }
    }

    var ref: GG_CoapMessageType {
        switch self {
        case .confirmable:
            return GG_COAP_MESSAGE_TYPE_CON
        case .nonConfirmable:
            return GG_COAP_MESSAGE_TYPE_NON
        case .acknowledgement:
            return GG_COAP_MESSAGE_TYPE_ACK
        case .reset:
            return GG_COAP_MESSAGE_TYPE_RST
        }
    }
}
