# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Tasks to build Golden Gate for the current platform"""

import os
import sys
import getpass
import time
import platform
from invoke import task, exceptions

from . import cmake

# auto-detect the host platform
def detect_profile():
    '''Automatically detect the profile based on the host platform'''
    profile = {
        'Darwin': 'macOS_cmdline',
        'Linux': 'linux',
        'Windows': 'windows'
    }.get(platform.system())
    if not profile:
        raise exceptions.PlatformError("Host platform not supported for native build")
    return profile

@task(help={
    "debug": "Build the Debug build instead of the Release build",
    "coverage": "Generate a code coverage report",
    "sonarqube": "Enable Sonarqube",
    "sanitize": "Enable a sanitizer ('address', ...). You can use this option multiple times, one for each sanitizer.",
    "cmakegen": "Override CMake generator (e.g. 'Xcode', run `cmake --help` for a list of supported generators)"
}, iterable=['sanitize'])
def build(ctx, debug=False, coverage=False, sonarqube=False, sanitize=None, cmake_verbose=False, cmakegen=None):
    '''Build Golden Gate natively using the default auto-detected profile for the host platform'''
    build_dir = ctx.C.BUILD_DIR_NATIVE
    cmake.build(ctx,
                build_dir,
                detect_profile(),
                tests=True,
                debug=debug,
                coverage=coverage,
                sanitize=sanitize,
                cmake_verbose=cmake_verbose,
                generator=cmakegen)
    build_wrapper = ""
    if sonarqube:
        wrapper_bin = {
            'Darwin': 'build-wrapper-macosx-x86',
            'Linux': 'build-wrapper-linux-x86-64'
        }.get(platform.system())
        if not wrapper_bin:
            raise exceptions.PlatformError("Host platform not supported for native build")
        build_wrapper = "{} --out-dir {}/sonarqube/build-wrapper ".format(wrapper_bin, build_dir)

        # Ensure the output directory exists
        ctx.run("mkdir -p {build_dir}/sonarqube/build-wrapper".format(build_dir=build_dir))

        # Check that the wrapper binary is installed
        try:
            ctx.run("{}true".format(build_wrapper), pty=(sys.platform != 'win32'))
        except exceptions.UnexpectedExit as e:
            print(type(e))
            print("!!! Sonarqube wrapper not installed ({}), will not run analysis. See https://docs.sonarqube.org/latest/analysis/languages/cfamily".format(wrapper_bin))
            build_wrapper = ""

    # Run the build
    ctx.run("{}cmake --build {}".format(build_wrapper, build_dir), pty=(sys.platform != 'win32'))

@task
def clean(ctx):
    '''Blow away the build directory'''
    ctx.run("rm -rf {}".format(ctx.C.BUILD_DIR_NATIVE))

@task
def test(ctx, coverage=False, sonarqube=False, output_on_failure=True, verbose=False, file_name=None, skip_build=True):
    '''Run GoldenGate XP tests
    Note: please run "inv native.build" to build the unit tests first

    Examples:
        $ inv native.test

          Run all unit tests

        $ inv native.test -f test_gg_timer

          Run the gg timer unit test

        $ inv native.test -c

          Run all unit tests and generate a coverage report using `gcovr`.
          Note: Please build the unit tests with -c option. i.e. inv native.build -c
          For more information: https://gcovr.com

        $ inv native.test -c -s

          Same as above, plus a Sonarqube report.
    '''

    if not skip_build:
        build(ctx, coverage=coverage, sonarqube=sonarqube)

    # setup coverage before running tests

    if coverage:
        ctx.run("rm -rf coverage")
        ctx.run("mkdir -p coverage/xp")
        # Generate initial coverage data to capture untested files
        ctx.run("lcov --directory xp --zerocounters -q")
        ctx.run("lcov --initial --directory xp -c -o coverage/xp/initial.info")

    # Run the native unit tests
    with ctx.cd(ctx.C.BUILD_DIR_NATIVE):
        args = ["--output-on-failure"] if output_on_failure else []
        args += ["--verbose"] if verbose else []
        args += ["--tests-regex", file_name] if file_name else []
        ctx.run("ctest {}".format(" ".join(args)))

    # analyze coverage after running tests
    if coverage:
        # Create code coverage analysis, merging initial coverage data.
        # Exclude external libraries, examples, apps, and some system files.
        # Generate html report and cobertura compatible xml for CI.
        ctx.run("lcov --directory xp -c -o coverage/xp/test.info")

        with ctx.cd("coverage/xp"):
            # Sometimes lcov fails with "lcov: WARNING: negative counts found in tracefile"
            if not ctx.run("lcov -a initial.info -a test.info -o all.info", warn=True):
                return

            excluded_patterns = " ".join([
                "*/external/*",
                "*/xp/examples/*",
                "*/xp/apps/*",
                "/usr/include/*",
            ])

            ctx.run("lcov -r all.info {} -o output.info".format(excluded_patterns))

        # Cleanup output to remove function signature lines as hit
        ctx.run("scripts/remove-function-lines.py coverage/xp/output.info coverage/xp/final.info")

        # Generate reports
        ctx.run("genhtml -o coverage/xp/html -t \"Golden Gate test coverage\" " +
                "--num-spaces 4 coverage/xp/final.info")
        ctx.run("scripts/lcov_cobertura/lcov_cobertura.py coverage/xp/final.info --base-dir xp " +
                "--output coverage/xp/cobertura.xml")

        # Run Sonarqube analysis
        if sonarqube:
            # Run gcov on all .gcno files
            gcov_working_dir = "xp/build/cmake/native"
            with ctx.cd(gcov_working_dir):
                excluded = ["apps", "build", "examples", "unit_tests", "external"]
                gcno_files = []
                for root, dirs, files in os.walk("xp/build/cmake/native", topdown=True):
                    dirs[:] = [d for d in dirs if d not in excluded]
                    for file in files:
                        if file.endswith(".gcno"):
                            gcno_files.append(os.path.join(root, file).replace(gcov_working_dir + "/", ""))
                for gcno_file in gcno_files:
                    c_file = gcno_file.replace(".gcno", ".c")
                    ctx.run("gcov {}".format(c_file))

            # Run the scanner
            ctx.run("sonar-scanner")
