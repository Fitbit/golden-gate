package com.fitbit.goldengate.bindings

/**
 * Object that bind to native implementation
 */
internal interface NativeReference {
    /** Pointer to native jni reference for this object*/
    val thisPointer: Long
}
