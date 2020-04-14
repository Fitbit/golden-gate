option(GG_LIBS_ENABLE_MODULE "Enable module library" TRUE)
if(NOT GG_LIBS_ENABLE_MODULE)
    return()
endif()

set(SOURCES gg_module.c)
set(HEADERS gg_module.h)

add_library(gg-module ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-module)

target_link_libraries(gg-module PRIVATE gg-common)

set_target_properties(gg-module PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-module EXPORT golden-gate
                          ARCHIVE DESTINATION lib
                          PUBLIC_HEADER DESTINATION include/xp/module)

# ports
include(ports/windows/CMakeLists.txt)

# define processor symbols for modules that need an init function to be called
# and make the submodule a dependency of gg-common
if (${GG_CMAKE_VERBOSE})
    message("Module Dependencies: init->" "${GG_MODULE_INIT_LIST}" " deps->" "${GG_MODULE_DEPS_LIST}")
endif()
if (DEFINED GG_MODULE_INIT_LIST)
    foreach(submodule ${GG_MODULE_INIT_LIST})
        target_compile_definitions(gg-module PRIVATE GG_CONFIG_MODULE_INIT_${submodule})
    endforeach()
endif()
if (DEFINED GG_MODULE_DEPS_LIST)
    foreach(submodule ${GG_MODULE_DEPS_LIST})
        target_link_libraries(gg-module PRIVATE ${submodule})
    endforeach()
endif()
