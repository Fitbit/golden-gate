# Copyright 2017-2021 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

include(${CMAKE_CURRENT_LIST_DIR}/darkhorse.cmake)

# MbedTLS options
set(GG_MBEDTLS_CONFIG "gg_dtls_morgan_config.h" CACHE STRING "" FORCE)

