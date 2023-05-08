// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings

import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock

fun mockDataSinkDataSource(dataSinkPtr: Long = 0, dataSourcePtr: Long = 0): DataSinkDataSource {
  return mock {
    on { getAsDataSinkPointer() } doReturn dataSinkPtr
    on { getAsDataSourcePointer() } doReturn dataSourcePtr
  }
}
