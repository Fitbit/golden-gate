package com.fitbit.goldengate.bindings.stack

sealed class StackEvent (val eventId: Int)  {
    object GattlinkSessionReady: StackEvent(0x676c732b) // gls+
    object GattlinkSessionResent: StackEvent(0x676c732d) // gls-
    data class GattlinkSessionStall(val stalledTime: Int): StackEvent(0x676c7323) // gls#
    object GattlinkBufferOverThreshold: StackEvent(0x676c622b)  // glb+
    object GattlinkBufferUnderThreshold: StackEvent(0x676c622d) // glb-
    object Unknown: StackEvent(-1)

    companion object {
        fun parseStackEvent(event: Int, data: Int): StackEvent {
            return when(event) {
                GattlinkSessionReady.eventId -> GattlinkSessionReady
                GattlinkSessionResent.eventId -> GattlinkSessionResent
                GattlinkSessionStall(data).eventId -> GattlinkSessionStall(data)
                GattlinkBufferOverThreshold.eventId -> GattlinkBufferOverThreshold
                GattlinkBufferUnderThreshold.eventId -> GattlinkBufferUnderThreshold
                else -> Unknown
            }
        }
    }
}