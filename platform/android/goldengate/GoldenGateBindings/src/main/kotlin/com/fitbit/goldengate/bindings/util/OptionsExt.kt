package com.fitbit.goldengate.bindings.util

import com.fitbit.goldengate.bindings.coap.data.BlockInfo
import com.fitbit.goldengate.bindings.coap.data.IntOptionValue
import com.fitbit.goldengate.bindings.coap.data.Option
import com.fitbit.goldengate.bindings.coap.data.OptionNumber
import com.fitbit.goldengate.bindings.coap.data.Options

fun Options.getBlock2BlockInfoOrNull(): BlockInfo? {
    val block2Option: Option? = this.firstOrNull { it.number == OptionNumber.BLOCK2 }

    return block2Option?.let { (it.value as IntOptionValue).value.toBlockInfo() }
}
