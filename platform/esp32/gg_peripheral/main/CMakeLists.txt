# GBG Experimental
cmake_policy(SET CMP0079 NEW)

idf_component_register(SRCS "main.c;gg_esp32_ble.c"
                       INCLUDE_DIRS ".")

# Build static library, do not build test executables
option(BUILD_SHARED_LIBS OFF)
option(BUILD_TESTING OFF)

# Unfortunately the library performs install and export. Would
# have been nice if devs made that an option like BUILD_SHARED_LIBS
# and BUILD_TESTING. Override install() and export() to do nothing
# instead.
function(install)
endfunction()

function(export)
endfunction()

# If GG_ROOT isn't set, set it relative to this directory
if (DEFINED $ENV{GG_ROOT})
    set(GG_ROOT $ENV{GG_ROOT})
else()
    set(GG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
endif()

# Import the Golden Gate lib
include(${GG_ROOT}/xp/config/profiles/esp32.cmake)
add_subdirectory(${GG_ROOT}/xp gg_build)

# When compiling Golden Gate's mbedtls support library, it needs
# to know where the mbedtls headers are
target_link_libraries(gg-tls PRIVATE idf::mbedtls)

# Link Golden Gate to main component
target_link_libraries(${COMPONENT_LIB} PUBLIC gg-runtime)
