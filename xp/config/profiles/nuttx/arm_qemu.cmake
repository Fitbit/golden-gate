# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

include(${CMAKE_CURRENT_LIST_DIR}/nuttx.cmake)

set(GG_PORTS_ENABLE_NULL_LOG_CONFIG TRUE CACHE BOOL "")
set(GG_PORTS_ENABLE_STDC_CONSOLE TRUE CACHE BOOL "")
