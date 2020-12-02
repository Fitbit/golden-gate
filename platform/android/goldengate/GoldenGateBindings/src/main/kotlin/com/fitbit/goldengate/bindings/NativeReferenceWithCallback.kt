package com.fitbit.goldengate.bindings

interface NativeReferenceWithCallback {

    /** Pointer to native jni reference for this object wrapper */
    var thisPointerWrapper: Long

    /**
     * invoke from native code to notify jvm the memory is going to be freed
     */
    fun onFree()
}
