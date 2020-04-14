option(GG_CONFIG_ANNOTATIONS_DISABLE_COMMS_ERRORS "Disable CommsError debug annotations" FALSE)
if (NOT GG_CONFIG_ENABLE_ANNOTATIONS)
    add_library(gg-annotations INTERFACE)
    target_include_directories(gg-annotations INTERFACE ${GG_ROOT}/xp/annotations)
    return()
endif()

# Root of the debug annotations
set(GG_DEBUG_ANNOTATIONS_SRC_ROOT ${GG_ROOT}/external/protobuf-debug-annotations/src)

# Compute the list of all .proto files we need to compile
file(GLOB GG_DEBUG_ANNOTATION_PROTO_FILES
     ${GG_DEBUG_ANNOTATIONS_SRC_ROOT}/*.proto
     ${GG_DEBUG_ANNOTATIONS_SRC_ROOT}/subtypes/*.proto)

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

# Create the targets for all annotations
NANOPB_GENERATE_CPP(PROTO_SRCS PROTO_HDRS
                    RELPATH ${GG_DEBUG_ANNOTATIONS_SRC_ROOT}
                    ${GG_DEBUG_ANNOTATION_PROTO_FILES})

# Remove the common NanoPB files, since they are built in a different module
set(SRC_FILES)
foreach(SRC_FILE ${PROTO_SRCS})
    get_filename_component(SRC_FILE_NAME ${SRC_FILE} NAME)
    if(NOT(((${SRC_FILE_NAME} STREQUAL "pb_common.c") OR
            (${SRC_FILE_NAME} STREQUAL "pb_encode.c") OR
            (${SRC_FILE_NAME} STREQUAL "pb_decode.c") OR
            (GG_CONFIG_ANNOTATIONS_ONLY_HEADERS))))
        list(APPEND SRC_FILES ${SRC_FILE})
    endif()
endforeach()

# Create a target for the library that contains all annotation types
add_library(gg-annotations gg_annotations.h ${SRC_FILES} ${PROTO_HDRS})

set_target_properties(gg-annotations PROPERTIES LINKER_LANGUAGE C)

target_include_directories(gg-annotations PUBLIC ${NANOPB_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
