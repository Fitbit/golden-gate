package com.fitbit.goldengate.bindings.sinksourceadapter

class SinkSourceAdapterSendProvider(private val sinkSourceAdapterDataSender: SinkSourceAdapterDataSender) {

    fun provide(): SinkSourceAdapterDataSender {
        return sinkSourceAdapterDataSender
    }
}
