# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

#############################################################################
# Top Level CMake file for the Golden Gate cross-platform library
#############################################################################

# We require some CMake 3.19 when building with XCode 12
# but the Android SDK ships with 3.6 so we can't force it globally (yet).
if (CMAKE_GENERATOR MATCHES "Xcode")
    cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
else ()
    cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
endif ()

# The project variant suffix is only in use on platforms
# that need two versions of golden-gate simultaneously.
# The variable should be defined in the profile for that
# variant
project(golden-gate${GG_PROJECT_VARIANT} VERSION 0.1.0 LANGUAGES C)

# Include some CMake macros definitions
include(CMakeDependentOption)

# Setup root directories
get_filename_component(GG_ROOT .. ABSOLUTE BASEDIR ${CMAKE_CURRENT_SOURCE_DIR})

# Global options
set(CMAKE_C_STANDARD 99)
add_definitions(-D_THREAD_SAFE)

# XCode-specific options
if (CMAKE_GENERATOR MATCHES "Xcode")
    set(XCODE_GENERATE_SCHEME TRUE)

    # The intermediate directories of Xcode don't work well with CMake
    # when we try to merge libraries into a single static library.
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib$(EFFECTIVE_PLATFORM_NAME))
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib$(EFFECTIVE_PLATFORM_NAME))
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin$(EFFECTIVE_PLATFORM_NAME))
endif ()

# Include dirs seen by all modules
include_directories(${GG_ROOT})
include_directories(${CMAKE_BINARY_DIR})

# Set default compiler warnings
if(GG_SANITIZE)
    string(REPLACE "," ";" GG_SANITIZE ${GG_SANITIZE})
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wall -Wextra)

    if(GG_COVERAGE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O0 -g -lgcov -fprofile-arcs -ftest-coverage")
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Weverything -Wint-conversion -Wno-padded -Wno-sign-conversion -Wno-extended-offsetof
    -Wno-documentation-unknown-command -Wno-documentation-deprecated-sync -Wno-missing-field-initializers
    -Wno-missing-noreturn -Wno-c99-extensions -Wno-vla -Wno-date-time -Wno-unknown-warning-option -Wno-disabled-macro-expansion -fno-common
    -Wno-missing-braces)
    if(CMAKE_OSX_SYSROOT)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lobjc")
    endif()
    if(GG_COVERAGE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 --coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(GG_SANITIZE)
        foreach(SANITIZER IN LISTS GG_SANITIZE)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${SANITIZER} -fno-omit-frame-pointer")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZER}")
        endforeach()
    endif()
endif()

# Target platform tools options
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # don't warn about files with no symbols
    set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

if (CMAKE_GENERATOR MATCHES "Xcode")
    # For some reason, setting this in the config subdir doesn't work, it has to be at the top level
    set(CMAKE_SUPPRESS_REGENERATION TRUE) # don't produce a ZERO_CHECK target in the Xcode project
endif()

# Target platform compile flags
if (DEFINED GG_PLATFORM)
    add_definitions(-DGG_CONFIG_PLATFORM=${GG_PLATFORM})
endif()

# Target platform include path
if (DEFINED GG_PLATFORM_INCLUDE_PATH)
    # convert whitespace-seaparated list from make into semicolon-separated list form cmake
    separate_arguments(GG_PLATFORM_INCLUDE_PATH)
    include_directories(${GG_PLATFORM_INCLUDE_PATH})
endif()

# Target platform defines
if (DEFINED GG_PLATFORM_DEFINES)
    # convert whitespace-seaparated list from make into semicolon-separated list form cmake
    separate_arguments(GG_PLATFORM_DEFINES)
    add_definitions(${GG_PLATFORM_DEFINES})
endif()

option(GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH "Loop message queue length" "")
if (GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH)
    add_definitions(-DGG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH=${GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH})
endif()

option(GG_CONFIG_MAX_TIMERS "Max number of timers" "")
if (GG_CONFIG_MAX_TIMERS)
    add_definitions(-DGG_CONFIG_MAX_TIMERS=${GG_CONFIG_MAX_TIMERS})
endif()

option(GG_CONFIG_ENABLE_LOGGING "Enable logging" TRUE)
if(GG_CONFIG_ENABLE_LOGGING)
    add_definitions(-DGG_CONFIG_ENABLE_LOGGING)
endif()

option(GG_CONFIG_LOGGING_MIN_LEVEL "Min logging level" "")
if(GG_CONFIG_LOGGING_MIN_LEVEL)
    add_definitions(-DGG_CONFIG_LOGGING_MIN_LEVEL=${GG_CONFIG_LOGGING_MIN_LEVEL})
endif()

option(GG_CONFIG_ENABLE_INSPECTION "Enable object inspection" TRUE)
if(GG_CONFIG_ENABLE_INSPECTION)
    add_definitions(-DGG_CONFIG_ENABLE_INSPECTION)
endif()

option(GG_CONFIG_ENABLE_THREAD_GUARDS "Enable thread guards" FALSE)
option(GG_CONFIG_THREAD_GUARDS_ASSERT "Thread guards should assert" FALSE)
if(GG_CONFIG_ENABLE_THREAD_GUARDS)
    add_definitions(-DGG_CONFIG_ENABLE_THREAD_GUARDS)
    if(GG_CONFIG_THREAD_GUARDS_ASSERT)
        add_definitions(-DGG_CONFIG_THREAD_GUARDS_ASSERT)
    endif()
endif()

option(GG_CONFIG_ENABLE_ANNOTATIONS "Enable debug annotations" FALSE)
if(GG_CONFIG_ENABLE_ANNOTATIONS)
    add_definitions(-DGG_CONFIG_ENABLE_ANNOTATIONS)
endif()

if (DEFINED GG_CONFIG_NANOPB_DIR_PATH)
    set(NANOPB_SRC_ROOT_FOLDER ${GG_CONFIG_NANOPB_DIR_PATH})
else()
    set(NANOPB_SRC_ROOT_FOLDER ${GG_ROOT}/external/nanopb)
endif()

# Macros
set(GG_ALL_LIBS, "")
macro(gg_add_to_all_libs library)
    list(APPEND GG_ALL_LIBS ${library})
    set(GG_ALL_LIBS ${GG_ALL_LIBS} PARENT_SCOPE)
endmacro()

# Normalize file path references in the c code to the golden-gate root directory
if (CMAKE_GENERATOR MATCHES "Unix Makefiles")
    # This pattern is only suitable for GNU Make
    set(GG_CPATH_SUBST "'\"$(subst $(abspath ${GG_ROOT})/,,$(abspath $<))\"'")
else()
    # Fall back to __FILE__ builtin
    message(STATUS "Build backend = ${CMAKE_GENERATOR}, not normalizing paths")
    set(GG_CPATH_SUBST "__FILE__")
endif()

# list(APPEND ...) shoves a `;` in front of this option, so just use
# old-fashioned set. add_definitions won't permit passing through the Make subst
# so set is easiest.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGG_BASE_FILE_NAME=${GG_CPATH_SUBST}")

# Library submodules
add_subdirectory(common)
add_subdirectory(sockets)
add_subdirectory(loop)
add_subdirectory(coap)
add_subdirectory(diagnostics)
add_subdirectory(gattlink)
add_subdirectory(remote)
add_subdirectory(protocol-spec)
add_subdirectory(protocols)
add_subdirectory(tls)
add_subdirectory(utils)
add_subdirectory(smo)
add_subdirectory(lwip)
add_subdirectory(mbedtls)
add_subdirectory(nip)
add_subdirectory(stack_builder)
add_subdirectory(nanopb)
add_subdirectory(annotations)
add_subdirectory(netif/nuttx)
add_subdirectory(services/blast)
add_subdirectory(services/diagnostics)
add_subdirectory(services/stack)
add_subdirectory(services/coap_client)
add_subdirectory(services/test_server)

#############################################
# commands below this separator should stay below all the add_subdirectory() calls
# because they rely on variables that may be set or modified by submodules
#############################################
add_subdirectory(module)

# Utilities for Android/iOS
add_subdirectory(bundle)

# Collect all submodule libs into one
add_library(gg-runtime INTERFACE)
target_link_libraries(gg-runtime INTERFACE ${GG_ALL_LIBS})
target_include_directories(gg-runtime INTERFACE ${GG_ROOT})

# Apps
option(GG_ENABLE_APPS "Enable building of applications" TRUE)
if(GG_ENABLE_APPS)
    add_subdirectory(apps/coap-client)
    add_subdirectory(apps/coap-server)
    add_subdirectory(apps/stack-tool)
endif()

# Examples
option(GG_ENABLE_EXAMPLES "Enable building of examples" TRUE)
if(GG_ENABLE_EXAMPLES)
    add_subdirectory(examples/interfaces)
    add_subdirectory(examples/logging)
    add_subdirectory(examples/sockets)
    add_subdirectory(examples/loop)
    add_subdirectory(examples/remote)
    add_subdirectory(examples/gattlink)
    add_subdirectory(examples/dtls)
    add_subdirectory(examples/coap)
    add_subdirectory(examples/service_host)
    add_subdirectory(examples/web_bluetooth)
    add_subdirectory(examples/inspect)
    add_subdirectory(examples/stack)
endif()

# Unit Tests
option(GG_ENABLE_UNIT_TESTS "Enable unit tests" TRUE)
if(GG_ENABLE_UNIT_TESTS)
    enable_testing()
    add_subdirectory(unit_tests)
endif()

# Documentation
if (GG_GENERATE_DOCS)
    add_subdirectory(docs)
endif()
