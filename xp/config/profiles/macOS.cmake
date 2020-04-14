# Inherit from shared settings
include(${CMAKE_CURRENT_LIST_DIR}/iOS_macOS.cmake)

# Enable/Disable groups
set(GG_ENABLE_APPS TRUE CACHE BOOL "")

# Build options
set(GG_PROJECT_VARIANT -macos CACHE INTERNAL "Suffix for xcodeproj directory")

# CMake options
set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS minimum version.")
set(CMAKE_OSX_SYSROOT "macosx" CACHE STRING "MacOS Latest SDK")
