# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

include(${CMAKE_CURRENT_LIST_DIR}/gcc_arm.cmake)
set(COMMON_FLAGS "-Os -fdata-sections -ffunction-sections -fno-exceptions -ggdb -mcpu=cortex-m4 -mthumb -mthumb-interwork --specs=nano.specs")
