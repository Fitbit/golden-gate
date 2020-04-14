package com.fitbit.goldengate.bindings.util

private const val ADDRESS_LENGTH = 17

/**
 * NOTE: This method is exact copy of BluetoothAdapter.checkBluetoothAddress method from Android library.
 * Reason to copy this is accessing BluetoothAdapter.checkBluetoothAddress method directly created some
 * problems running tests that depend on loading of GG XP library, even after annotating such test classes
 * to run with RobolectricTestRunner
 *
 *
 * Validate a String Bluetooth address, such as "00:43:A8:23:10:F0"
 *
 * Alphabetic characters must be uppercase to be valid.
 *
 * @param address Bluetooth address as string
 * @return true if the address is valid, false otherwise
 */
internal fun checkBluetoothAddress(address: String?): Boolean {
    if (address == null || address.length != ADDRESS_LENGTH) {
        return false
    }
    loop@ for (i in 0 until ADDRESS_LENGTH) {
        val c = address[i]
        when (i % 3) {
            0, 1 -> {
                if ((c in '0'..'9') || (c in 'A'..'F')) {
                    // hex character, OK
                    continue@loop
                }
                return false
            }
            2 -> {
                if (c == ':') {
                    continue@loop  // OK
                }
                return false
            }
        }
    }
    return true
}