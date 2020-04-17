# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

option(GG_LIBS_ENABLE_SMO "Enable SMO library" TRUE)
option(GG_LIBS_BUILD_SMO_ALLOCATOR_ONLY "Only build SMO allocator" FALSE)

if(NOT GG_LIBS_ENABLE_SMO)
    return()
endif()

set(SOURCES gg_smo_allocator.c)
set(HEADERS gg_smo_allocator.h)

add_library(gg-smo ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-smo)

set(GG_SMO_C_LIB_ROOT ${GG_ROOT}/external/smo/c/lib)

if(NOT GG_LIBS_BUILD_SMO_ALLOCATOR_ONLY)
    file(GLOB SMO_SRC_FILES ${GG_SMO_C_LIB_ROOT}/*.c)
    file(GLOB PUBLIC_HEADERS ${GG_SMO_C_LIB_ROOT}/inc/*.h)
    target_sources(gg-smo PRIVATE ${SMO_SRC_FILES})
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
	target_compile_options(gg-smo PRIVATE -Wno-unused-macros -Wno-covered-switch-default -Wno-double-promotion -Wno-conversion -Wno-cast-qual)
endif()

target_include_directories(gg-smo PUBLIC ${GG_SMO_C_LIB_ROOT}/inc)

set_target_properties(gg-smo PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-smo EXPORT golden-gate
                       ARCHIVE DESTINATION lib
                       PUBLIC_HEADER DESTINATION include/xp/smo)
