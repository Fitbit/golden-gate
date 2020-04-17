// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.client.services

import java.util.UUID

/**
 * Fitbit GATT database confirmation service, hosted by node
 *
 */
class GattDatabaseConfirmationService {

    companion object {
        val uuid: UUID = UUID.fromString("AC2F0045-8182-4BE5-91E0-2992E6B40EBB")
    }

    class EphemeralCharacteristicPointer {
        companion object {
            val uuid: UUID = UUID.fromString("AC2F0145-8182-4BE5-91E0-2992E6B40EBB")
        }
    }
}
