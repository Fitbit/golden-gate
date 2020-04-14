package com.fitbit.linkcontroller.services.status

import java.util.UUID

/**
 * Represents a Bluetooth GATT Link Status Service hosted on the tracker that allows the mobile app to
 * receive the current connection parameters and connection mode.
 */
class LinkStatusService{
    companion object {
        val uuid = UUID.fromString("ABBAFD00-E56A-484C-B832-8B17CF6CBFE8")
        val currentConfigurationUuid = UUID.fromString("ABBAFD01-E56A-484C-B832-8B17CF6CBFE8")
        val currentConnectionModeUuid = UUID.fromString("ABBAFD02-E56A-484C-B832-8B17CF6CBFE8")
    }
}
