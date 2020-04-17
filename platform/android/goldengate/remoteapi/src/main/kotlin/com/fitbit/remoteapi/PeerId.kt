// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

/**
 * Enum representing a peer id type
 */
enum class PeerIdType(val value: String) {
    NAME("name"),
    PRODUCT_ID("productId"),
    WIRE_ID("wireId"),
    MAC_ADDRESS("macAddress");

    companion object {
        fun fromString(value: String) = PeerIdType.values().firstOrNull { it.value == value }
                ?: throw IllegalArgumentException("The value '$value' provided does not match an enum value")
    }
}

/**
 * Data class identifying a peer through it's identifier and [PeerIdType]
 * @param id The id
 * @param type The type
 */
data class PeerId(val id:String, val type:PeerIdType)
