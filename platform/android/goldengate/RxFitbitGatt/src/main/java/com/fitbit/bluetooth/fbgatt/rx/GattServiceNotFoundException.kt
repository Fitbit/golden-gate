package com.fitbit.bluetooth.fbgatt.rx

import java.util.UUID

/**
 * Exception indicating GATT service not found
 *
 * @param uuid id of GATT service that was not found
 */
class GattServiceNotFoundException(uuid: UUID) : Exception("GATT service with uuid: $uuid, not found")
