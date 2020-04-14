package com.fitbit.goldengate.bindings.stack

import androidx.annotation.Keep

/**
 * Hold result for Stack creation
 */
@Keep
internal data class StackCreationResult(
    /**
     * non-negative if stack was successfully created, if -ve stack creation failed and
     * result will contain error code
     */
    val result: Int,
    /**
     * Native reference to stack pointer
     */
    val stackPointer: Long
)
