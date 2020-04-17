# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_LOGGING_EXAMPLE "Enable logging example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_LOGGING_EXAMPLE)
    return()
endif()

add_executable(gg-logging-example gg_logging_example.c gg_logging_example.h
                                  gg_logging_example_lib.c
                                  gg_logging_example_lib2.c)

target_link_libraries(gg-logging-example PRIVATE gg-runtime gg-annotations)
if (GG_CONFIG_ENABLE_ANNOTATIONS)
    target_link_libraries(gg-logging-example PRIVATE gg-nanopb)
endif()
