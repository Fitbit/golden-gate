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

    cmake_version = "3.6.4111459"
    cmake_directory = "{SDK}/cmake/{VERSION}".format(SDK=ctx.android.sdk, VERSION=cmake_version)
    cmake_bin = "{CMAKE_DIRECTORY}/bin/cmake".format(CMAKE_DIRECTORY=cmake_directory)
    if not os.path.isdir(cmake_directory):
        print("Cannot find CMake version:", cmake_version, "from the directory:", cmake_directory)
        raise Exit(1)

    build_dir = os.path.join(ctx.C.BUILD_DIR, "android")

    if profile is None:
        profiles = ["armeabi-v7a", "arm64-v8a", "x86", "x86_64"]
    else:
        profiles = [profile]

    toolchain = "{}/ndk-bundle/build/cmake/android.toolchain.cmake".format(ctx.android.sdk)
    generator = "'Android Gradle - Ninja'"

    for arch in profiles:
        profile = "android/{}".format(arch)
        arch_dir = os.path.join(build_dir, arch)
        cmake_extras = "-DGG_CONFIG_ENABLE_LOGGING=False" if release else None
        cmake.build(
            ctx, arch_dir, profile, toolchain_file=toolchain,
            generator=generator, cmake_bin=cmake_bin, cmake_extras=cmake_extras,
            cmake_verbose=cmake_verbose
        )
        ctx.run("cmake --build {build}".format(build=arch_dir), pty=True)
        # Gradle doesn't know how to find this .so when it's in the subdirectory
        symlink_dest = os.path.join(arch_dir, "libgg.so")
        ctx.run("test -f {symlink_dest} && rm {symlink_dest} || true".format(
            symlink_dest=symlink_dest))
        ctx.run("ln -s {source} {dest}".format(
            source=os.path.join(arch_dir, "bundle", "libgg.so"),
            dest=symlink_dest))
