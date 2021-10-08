# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"Android Library tasks"
import os

from invoke import task
from invoke.exceptions import Exit

from .. import cmake

@task
def build(ctx, release=False, profile=None, cmake_verbose=False):
    """Build Golden Gate for Android"""
    # We use a version of CMake bundled with the Android NDK. Google has forked CMake to add
    # Android-specific generators. The mainline CMake includes Android support from 3.7 onward,
    # but this is currently a preview feature.
    if ctx.android.sdk is None:
        print("""
Please set the ANDROID_HOME environment variable. The directory it points to
should have a cmake subdirectory. If not, install NDK, CMake, and LLDB from
the SDK manager.
""")
        raise Exit(1)

    build_dir = os.path.join(ctx.C.BUILD_DIR, "android")

    if profile is None:
        profiles = ["armeabi-v7a", "arm64-v8a", "x86", "x86_64"]
    else:
        profiles = [profile]

    ndk_version = "23.0.7599858"
    toolchain = "{}/ndk/{}/build/cmake/android.toolchain.cmake".format(ctx.android.sdk, ndk_version)
    generator = "'Ninja'"

    for arch in profiles:
        profile = "android/{}".format(arch)
        arch_dir = os.path.join(build_dir, arch)
        cmake_extras = "-DGG_CONFIG_ENABLE_LOGGING=False" if release else None
        cmake.build(
            ctx, arch_dir, profile, toolchain_file=toolchain,
            generator=generator, cmake_extras=cmake_extras,
            cmake_verbose=cmake_verbose
        )
        ctx.run("cmake --build {build}".format(build=arch_dir), pty=True)
