CMAKE_DEPENDENT_OPTION(GG_APPS_ENABLE_WEB_BLUETOOTH_EXAMPLE "Enable Web Bluetooth example" ON "GG_ENABLE_EXAMPLES" OFF)
if(NOT GG_APPS_ENABLE_WEB_BLUETOOTH_EXAMPLE OR NOT EMSCRIPTEN)
    return()
endif()

set(WEB_BLUETOOTH_HTML_SHELL ${GG_ROOT}/xp/examples/web_bluetooth/web_bluetooth_example_shell.html)
add_executable(gg-web-bluetooth-example web_bluetooth_example.c)
target_link_libraries(gg-web-bluetooth-example PRIVATE gg-runtime)
target_compile_options(gg-web-bluetooth-example PRIVATE -Wno-missing-prototypes)
set_target_properties(gg-web-bluetooth-example
                      PROPERTIES
                      SUFFIX ".html"
                      LINK_FLAGS "--shell-file ${WEB_BLUETOOTH_HTML_SHELL} -s EXPORTED_FUNCTIONS='[\"_web_bluetooth_initialize\",\"_web_bluetooth_terminate\", \"_web_bluetooth_do_work\"]'"
                      LINK_DEPENDS ${WEB_BLUETOOTH_HTML_SHELL})

add_custom_target(web-blutooth-bindings-file ALL
                  COMMAND ${CMAKE_COMMAND} -E create_symlink ${GG_ROOT}/xp/examples/web_bluetooth/web_bluetooth_bindings.js web_bluetooth_bindings.js)
add_dependencies(gg-web-bluetooth-example web-blutooth-bindings-file)
