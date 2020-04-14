CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_STACK_EXAMPLE "Enable Stack example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_STACK_EXAMPLE)
    return()
endif()

if (GG_PORTS_ENABLE_BSD_SOCKETS)
    add_executable(gg-multi-stack-example multi_stack_example.c)
    target_link_libraries(gg-multi-stack-example PRIVATE gg-runtime)
endif()
