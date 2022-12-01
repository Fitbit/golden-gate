package com.fitbit.goldengate.bindings.sinksourceadapter

class SinkSourceAdapterReceiveProvider(private val sinkSourceAdapterDataReceiver: SinkSourceAdapterDataReceiver) {

    fun provide(): SinkSourceAdapterDataReceiver {
        return sinkSourceAdapterDataReceiver
    }
}
