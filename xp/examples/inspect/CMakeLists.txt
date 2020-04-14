CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_EXAMPLE_INSPECT "Enable inspect example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_EXAMPLE_INSPECT)
    return()
endif()

add_executable(gg-inspect-example inspect_example.c)
target_link_libraries(gg-inspect-example PRIVATE gg-runtime)

