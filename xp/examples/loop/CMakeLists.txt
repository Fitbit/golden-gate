CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_LOOP_EXAMPLE "Enable loop examples" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_LOOP_EXAMPLE)
    return()
endif()

if (GG_PORTS_ENABLE_POSIX_THREADS)
    add_executable(gg-multithread-loop-example multithread_loop_example.c)
    target_link_libraries(gg-multithread-loop-example PRIVATE gg-runtime)

    add_executable(gg-data-sink-proxy-example data_sink_proxy_example.c)
    target_link_libraries(gg-data-sink-proxy-example PRIVATE gg-runtime)

    add_executable(gg-data-sink-proxy-listener-example data_sink_listener_proxy_example.c)
    target_link_libraries(gg-data-sink-proxy-listener-example PRIVATE gg-runtime)

    add_executable(gg-invoke-example invoke_example.c)
    target_link_libraries(gg-invoke-example PRIVATE gg-runtime)
endif()

if (EMSCRIPTEN)
    set(LOOP_EXAMPLE_HTML_SHELL ${GG_ROOT}/xp/examples/loop/wasm_loop_example_shell.html)
    add_executable(gg-wasm-loop-example wasm_loop_example.c)
    target_compile_options(gg-wasm-loop-example PRIVATE -Wno-missing-prototypes)
    target_link_libraries(gg-wasm-loop-example PRIVATE gg-runtime)
    set_target_properties(gg-wasm-loop-example PROPERTIES
                          SUFFIX ".html"
                          LINK_FLAGS "--shell-file ${LOOP_EXAMPLE_HTML_SHELL} -s EXPORTED_FUNCTIONS='[\"_main\",\"_do_work\"]'"
                          LINK_DEPENDS ${LOOP_EXAMPLE_HTML_SHELL})
endif()

add_executable(gg-timers-loop-example timers_loop_example.c)
target_link_libraries(gg-timers-loop-example PRIVATE gg-runtime)

