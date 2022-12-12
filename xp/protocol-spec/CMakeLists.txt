# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_PROTOCOL_SPEC "Enable protocol spec library" FALSE)
option(GG_CONFIG_ENABLE_PROTOCOL_SPEC_NANOPB "Include nanopb in protocol spec library" FALSE)
if (NOT GG_LIBS_ENABLE_PROTOCOL_SPEC)
    return()
endif()

# Root of the protocol-spec dir
set(GG_PROTOCOL_SPEC_SRC_ROOT ${GG_ROOT}/external/protocol-spec/spec)

# Compute the list of all .proto files we need to compile
file(GLOB GG_PROTOCOL_SPEC_PROTO_FILES ${GG_PROTOCOL_SPEC_SRC_ROOT}/*.proto)

# We need to find the protoc compiler and NanoPB in the host, not the target
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE_BAK ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM_BAK ${CMAKE_FIND_ROOT_PATH_MODE_PROGRAM})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Use the CMake support from Nanopb
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${NANOPB_SRC_ROOT_FOLDER}/extra)
find_package(Nanopb REQUIRED)

# Reset the search mode to its original value
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ${CMAKE_FIND_ROOT_PATH_MODE_INCLUDE_BAK})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ${CMAKE_FIND_ROOT_PATH_MODE_PROGRAM_BAK})

# By not including the generated timestamp in the protobuf files, we can commit
# the generated `.pb.c` and `.pb.h` to source control
# Note: After nanopb version 0.4.0, the default behavior is `--no-timestamp`
set(NANOPB_OPTIONS "--no-timestamp")

# Create the targets for all annotations
NANOPB_GENERATE_CPP(PROTO_SRCS PROTO_HDRS
                    RELPATH ${GG_PROTOCOL_SPEC_SRC_ROOT}
                    ${GG_PROTOCOL_SPEC_PROTO_FILES})

# Remove the common NanoPB files, since they are built in a different module
set(SRC_FILES)
foreach(SRC_FILE ${PROTO_SRCS})
    get_filename_component(SRC_FILE_NAME ${SRC_FILE} NAME)
    if(GG_CONFIG_ENABLE_PROTOCOL_SPEC_NANOPB OR
       NOT(((${SRC_FILE_NAME} STREQUAL "pb_common.c") OR
            (${SRC_FILE_NAME} STREQUAL "pb_encode.c") OR
            (${SRC_FILE_NAME} STREQUAL "pb_decode.c"))))
        list(APPEND SRC_FILES ${SRC_FILE})
    endif()
endforeach()

add_definitions(-DPB_FIELD_16BIT)

# Create a target for the library that contains all annotation types
add_library(gg-protocol-spec ${SRC_FILES} ${PROTO_HDRS})

# Add dependency to gg-annotations to avoid parallel build failures
# caused by NANOPB_GENERATE_CPP
add_dependencies(gg-protocol-spec gg-annotations)

target_include_directories(gg-protocol-spec PUBLIC ${NANOPB_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
