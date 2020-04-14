option(GG_LIBS_ENABLE_COMMON "Enable common library" TRUE)
if(NOT GG_LIBS_ENABLE_COMMON)
    return()
endif()

set(SOURCES gg_buffer.c
            gg_common.c
            gg_crc32.c
            gg_io.c
            gg_logging.c
            gg_memory.c
            gg_ring_buffer.c
            gg_strings.c
            gg_threads.c
            gg_timer.c
            gg_types.c
            gg_event_dispatcher.c
            gg_events.c
            gg_utils.c
            gg_inspect.c
            gg_bitstream.c
            gg_version.c)

set(HEADERS gg_buffer.h
            gg_common.h
            gg_crc32.h
            gg_events.h
            gg_io.h
            gg_lists.h
            gg_logging.h
            gg_memory.h
            gg_ring_buffer.h
            gg_port.h
            gg_queues.h
            gg_results.h
            gg_strings.h
            gg_system.h
            gg_threads.h
            gg_timer.h
            gg_types.h
            gg_event_dispatcher.h
            gg_events.h
            gg_utils.h
            gg_inspect.h
            gg_bitstream.h
            gg_version.h)

add_library(gg-common ${SOURCES} ${HEADERS})
gg_add_to_all_libs(gg-common)

# Include ports subdirectories
include(ports/stdc/CMakeLists.txt)
include(ports/posix/CMakeLists.txt)
include(ports/apple/CMakeLists.txt)
include(ports/android/CMakeLists.txt)
include(ports/windows/CMakeLists.txt)
include(ports/freertos/CMakeLists.txt)
include(ports/null/CMakeLists.txt)

# the log manager needs an init function to be called
list(APPEND GG_MODULE_INIT_LIST COMMON)
set(GG_MODULE_INIT_LIST ${GG_MODULE_INIT_LIST} PARENT_SCOPE)

set_target_properties(gg-common PROPERTIES PUBLIC_HEADER "${HEADERS}")
install(TARGETS gg-common EXPORT golden-gate
                          ARCHIVE DESTINATION lib
                          PUBLIC_HEADER DESTINATION include/xp/common)

# capture git commit hash for build number
if (NOT GIT_COMMIT_HASH)
  if(EXISTS "${GG_ROOT}/.git")
    execute_process(
      COMMAND git log -1 --format=%h
      WORKING_DIRECTORY ${GG_ROOT}
      OUTPUT_VARIABLE GG_GIT_COMMIT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
      COMMAND git diff-index --quiet HEAD
      WORKING_DIRECTORY ${GG_ROOT}
      RESULT_VARIABLE GG_LOCAL_MODS
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GG_LOCAL_MODS)
      string(APPEND GG_GIT_COMMIT_HASH "*")
    endif(GG_LOCAL_MODS)
    execute_process(
      COMMAND git rev-parse --abbrev-ref HEAD
      WORKING_DIRECTORY ${GG_ROOT}
      OUTPUT_VARIABLE GG_GIT_BRANCH_NAME
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
      COMMAND git log --oneline --first-parent  # list commits
      COMMAND wc -l                             # count commits
      COMMAND tr -d " "                         # strip leading whitespace
      WORKING_DIRECTORY ${GG_ROOT}
      OUTPUT_VARIABLE GG_GIT_COMMIT_COUNT
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (NOT GG_GIT_COMMIT_COUNT)
        set(GG_GIT_COMMIT_COUNT 0) # default value (for Windows, where 'wc' and 'tr' may not be available)
    endif()
  else(EXISTS "${GG_ROOT}/.git")
    message(FATAL_ERROR "GIT_COMMIT_HASH unavailable")
  endif(EXISTS "${GG_ROOT}/.git")
endif (NOT GIT_COMMIT_HASH)

message(STATUS "Captured Git commit info: hash=${GG_GIT_COMMIT_HASH} branch=${GG_GIT_BRANCH_NAME} commit_count=${GG_GIT_COMMIT_COUNT}")
configure_file(
    ${GG_ROOT}/xp/common/gg_version_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/xp/common/gg_version_info.h
)
target_include_directories(gg-common PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
