# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

set(TINYDTLS_ROOT ${PROJECT_SOURCE_DIR}/../../external/tinydtls)

file(GLOB TINYDTLS_SRC_FILES ${TINYDTLS_ROOT}/*.c
                             ${TINYDTLS_ROOT}/aes/*.c)

add_library(gg-tinydtls ${TINYDTLS_SRC_FILES})

target_include_directories(gg-tinydtls PUBLIC ${TINYDTLS_ROOT}
                                       PRIVATE ${TINYDTLS_ROOT}/aes
                                               config/dh-mini-experiment)
