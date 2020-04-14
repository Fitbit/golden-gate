# Download and unpack googletest at configure time
configure_file(${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM} .
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# This needs to because on some platforms this defaults to false and cmake won't build
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                 ${CMAKE_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

target_link_libraries(xp gtest gmock)

target_sources( # The name of the library to target sources to.
             xp

             # Sets the visibility for other sources.
             PUBLIC

             # list files of tests here
             ${CMAKE_CURRENT_LIST_DIR}/jni_gg_test_harness.cpp
             ${CMAKE_CURRENT_LIST_DIR}/jni_gg_test.cpp
             ${CMAKE_CURRENT_LIST_DIR}/util/jni_gg_util_test.cpp
             )

target_include_directories(xp PUBLIC
                           ${CMAKE_CURRENT_LIST_DIR})
