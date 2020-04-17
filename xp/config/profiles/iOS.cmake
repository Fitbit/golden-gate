# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

# Inherit from shared settings
include(${CMAKE_CURRENT_LIST_DIR}/iOS_macOS.cmake)

# Enable/Disable groups
set(GG_ENABLE_APPS FALSE CACHE BOOL "")
set(GG_ENABLE_UNIT_TESTS FALSE CACHE BOOL "")

# Build options
set(GG_PROJECT_VARIANT -ios CACHE INTERNAL "Suffix for xcodeproj directory")

# CMake options
set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "9.0" CACHE STRING "iOS minimum version.")
set(CMAKE_IOS_SDK_ROOT "iphoneos" CACHE STRING "iOS Latest SDK")
