package com.fitbit.goldengate.bindings

import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock

fun mockDataSinkDataSource(
    dataSinkPtr: Long = 0,
    dataSourcePtr: Long = 0
): DataSinkDataSource {
    return mock {
        on { getAsDataSinkPointer() } doReturn dataSinkPtr
        on { getAsDataSourcePointer() } doReturn dataSourcePtr
    }
}
