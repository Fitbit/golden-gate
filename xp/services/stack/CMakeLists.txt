# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_STACK_SERVICE "Enable stack service" TRUE)
if(NOT GG_LIBS_ENABLE_STACK_SERVICE)
    return()
endif()

add_library(gg-stack-service gg_stack_service.c gg_stack_service.h)
target_link_libraries(gg-stack-service PRIVATE gg-common PUBLIC gg-remote)

set_target_properties(gg-stack-service PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-stack-service EXPORT golden-gate
                                 ARCHIVE DESTINATION lib
                                 PUBLIC_HEADER DESTINATION include/xp/services/stack)
