include(${CMAKE_CURRENT_LIST_DIR}/nuttx.cmake)

# Platform options
set(GG_CONFIG_C_STANDARD "11" CACHE STRING "")
set(GG_PLATFORM GG_PLATFORM_DARKHORSE CACHE STRING "")

set(GG_CONFIG_LOOP_MESSAGE_QUEUE_LENGTH "16" CACHE STRING "")
set(GG_CONFIG_MAX_TIMERS "32" CACHE STRING "")
set(GG_PORTS_ENABLE_NANOPB FALSE CACHE BOOL "" FORCE)
set(GG_CONFIG_ANNOTATIONS_ONLY_HEADERS TRUE CACHE BOOL "")
set(GG_LIBS_ENABLE_PROTOCOL_SPEC TRUE CACHE BOOL "")
set(GG_CONFIG_ENABLE_INSPECTION FALSE CACHE BOOL "")
set(GG_CONFIG_ENABLE_ANNOTATIONS TRUE CACHE BOOL "")
