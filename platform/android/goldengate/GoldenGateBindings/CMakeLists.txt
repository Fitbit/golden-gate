# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

set(gg_DIR ${CMAKE_SOURCE_DIR}/../../../../)
set(cpp_DIR src/main/cpp/)

# This lets us compile the code for Linux/OSX/Cygwin for unit tests by pointing it to the JNI headers
if (NOT ANDROID)
    find_package(JNI REQUIRED)
    if (APPLE)
        set(platform "darwin")
        set(library_extension "dylib")
    elseif(UNIX)
        set(platform "linux")
        set(library_extension "so")
    else()
        set(platform "win32")
        set(library_extension "dll") # Not sure if this is correct.
    endif()
    include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH}/${platform})
else()
include_directories(${JNI_INCLUDE_DIRS})
endif()

option( GG_COVERAGE "Enable coverage" FALSE )

add_library( # Sets the name of the library.
             xp
             # Sets the library as a shared library.
             SHARED
             ""
             )

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

if( GG_COVERAGE )
    if ( CMAKE_C_COMPILER_ID MATCHES "Clang" )
        set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 --coverage" )
        set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 --coverage" )
        set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage" )
    else()
        set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage" )
        set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage" )
        set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov -fprofile-arcs -ftest-coverage" )
    endif()
endif()

# the compiler used for C files
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} )

# the compiler used for C++ files
MESSAGE( STATUS "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )

# the compiler flags for compiling C sources
MESSAGE( STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} )

# the compiler flags for compiling C++ sources
MESSAGE( STATUS "CMAKE_CXX_FLAGS: " ${CMAKE_CXX_FLAGS} )

add_library(libgg SHARED IMPORTED)

if (NOT ANDROID)
set_target_properties(libgg PROPERTIES IMPORTED_LOCATION
    ${gg_DIR}/xp/build/cmake/native/bundle/libgg.${library_extension})
else()
set_target_properties(libgg PROPERTIES IMPORTED_LOCATION
    ${gg_DIR}/xp/build/cmake/android/${ANDROID_ABI}/libgg.so)
endif()

# build application's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")

target_include_directories(xp PRIVATE
                           ${gg_DIR}/ ${gg_DIR}/external/smo/c/lib/inc/)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

set(libs_to_link "")

list(APPEND libs_to_link libgg)
if (ANDROID)
    list(APPEND libs_to_link android)
    list (APPEND libs_to_link ${log-lib})
endif()

target_link_libraries( # Specifies the target library.
                       xp
                       ${libs_to_link} )

include(src/main/cpp/CMakeLists.txt)

option(GG_ENABLE_UNIT_TESTS "Enable unit tests" FALSE)
if(NOT ANDROID AND GG_ENABLE_UNIT_TESTS)
    enable_testing()
    include(src/test/cpp/CMakeLists.txt)
endif()
