package com.fitbit.goldengate.bindings.sinksourceadapter

import io.reactivex.Completable

interface SinkSourceAdapterDataSender {
    fun putData(data: ByteArray): Completable
}
