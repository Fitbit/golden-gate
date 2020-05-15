# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

find_package(Doxygen
             OPTIONAL_COMPONENTS dot mscgen dia)
if(NOT DOXYGEN_FOUND)
    return()
endif()

# Add targets here to include their headers in docs
set(DOXYGEN_TARGETS gg-common
                    gg-gattlink
                    gg-protocols
                    gg-tls
                    gg-coap
                    gg-stack-builder
                    gg-loop
                    gg-utils
                    gg-sockets
                    gg-protocols
                    gg-nip
                    gg-module
                    gg-lwip)

# This combs through sources associated with targets and grabs
# header files
unset(DOXYGEN_INPUTS)
foreach(TARGET ${DOXYGEN_TARGETS})
    message(STATUS "Target: ${TARGET}")
    get_target_property(T_SOURCES ${TARGET} SOURCES)
    get_target_property(T_DIR ${TARGET} SOURCE_DIR)
    foreach(SOURCE ${T_SOURCES})
        if(${SOURCE} MATCHES "\.h$")
            list(APPEND DOXYGEN_INPUTS "${T_DIR}/${SOURCE}")
        endif()
    endforeach()
endforeach()
string(REPLACE ";" " " DOXYGEN_INPUTS "${DOXYGEN_INPUTS}")
message(STATUS "Doxy inputs: ${DOXYGEN_INPUTS}")

if(CMAKE_VERBOSE_MAKEFILE)
    set(DOXYGEN_QUIET NO)
else()
    set(DOXYGEN_QUIET YES)
endif()

set(DOXYGEN_OUTPUT_DIR ${GG_ROOT}/docs/src/api/doxygen)

configure_file(Doxyfile.in Doxyfile)
add_custom_command(OUTPUT doxygen.stamp
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                   COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                   COMMAND cmake -E touch ${CMAKE_CURRENT_BINARY_DIR}/doxygen.stamp
                   COMMENT "Generating API Docs"
                   VERBATIM)
add_custom_target(doxygen ALL DEPENDS doxygen.stamp)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs
        DESTINATION share/golden-gate)
