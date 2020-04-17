// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

/**
 * Enum representing CoAP group filter mode
 * CoAP resource filter can support up to four different filter groups.
 */
enum class CoapGroupRequestFilterMode(val value: Byte) {
    GROUP_0(0), //group #0 for authenticated states (no filtering)
    GROUP_1(1), //group #1 for non-authenticated connections
    GROUP_2(2), //group #2 reserved
    GROUP_3(3), //group #3 reserved
    GROUP_4(4)  //group #4 reserved
}
