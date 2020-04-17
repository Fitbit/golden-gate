# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_STACK_TOOL "Enable Stack Tool" ON "GG_ENABLE_APPS" OFF)
if(NOT GG_APPS_ENABLE_STACK_TOOL)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-stack-tool gg_stack_tool.c)
    target_link_libraries(gg-stack-tool PRIVATE gg-runtime)

    # add CoreBluetooth and tunnel support on Apple platforms
    if (APPLE)
        target_sources(gg-stack-tool PRIVATE
                       gg_stack_tool_core_bluetooth_transport.m
                       gg_stack_tool_core_bluetooth_transport.h
                       gg_stack_tool_macos_tunnel.c
                       gg_stack_tool_macos_tunnel.h)
        target_compile_definitions(gg-stack-tool PRIVATE
                                   -DGG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT
                                   -DGG_CONFIG_ENABLE_MACOS_TUNNEL)
        target_link_libraries(gg-stack-tool PRIVATE "-framework CoreBluetooth")
        set_target_properties(gg-stack-tool PROPERTIES LINK_FLAGS "-fobjc-link-runtime")
        target_compile_options(gg-stack-tool PRIVATE "-Wno-objc-missing-property-synthesis")
    endif()
endif()

