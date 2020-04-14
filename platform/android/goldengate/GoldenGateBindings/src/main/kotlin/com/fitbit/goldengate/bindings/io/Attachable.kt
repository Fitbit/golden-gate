package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.DataSinkDataSource

/**
 * A [Attachable] is a source or sink that can be attached to another source or sink.
 */
interface Attachable {

    /**
     * Attach the source and sink
     *
     * @param dataSinkDataSource source and sink for sending and receiving data
     */
    fun attach(dataSinkDataSource: DataSinkDataSource)
}
