# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Root for this port subdirectory
set(PORT_DIR ports/nuttx)

option(GG_PORTS_ENABLE_NUTTX_SEMAPHORE "Enable Nuttx Semaphore support" FALSE)
if(GG_PORTS_ENABLE_NUTTX_SEMAPHORE)
    target_sources(gg-common PRIVATE ${PORT_DIR}/gg_nuttx_semaphore.c)
endif()

