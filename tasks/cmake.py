# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import subprocess

def build(ctx, build_dir, profile, toolchain_file=None,
          generator=None, tests=False, debug=False, coverage=False, sanitize=False,
          docs=False,
          cmake_bin="cmake", cmake_verbose=None, cmake_wrapper=None, cmake_extras=None):
    '''Build Golden Gate using CMake'''
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    cmd = [cmake_bin]
    if cmake_wrapper:
        cmd = [cmake_wrapper] + cmd

    # check if Ninja is available, and use the Ninja generator if it is, unless a
    # custom generator was requested, or the host is Windows
    if not generator and sys.platform != 'win32':
        try:
            subprocess.check_output(['ninja', '--version'])
            cmd.append('-GNinja')
        except:   # pylint: disable-msg=W0702
            pass

    if cmake_verbose:
        cmd.append("-L")
        cmd.append("-DGG_CMAKE_VERBOSE=1")
    if debug:
        cmd.append("-DCMAKE_BUILD_TYPE=Debug")
    cmd.append("-Hxp")
    cmd.append("-B{}".format(build_dir))
    cmd.append("-Cxp/config/profiles/{}.cmake".format(profile))
    cmd.append("-DCMAKE_INSTALL_PREFIX={}".format(os.path.join(ctx.C.ROOT_DIR, "install")))
    if toolchain_file:
        cmd.append("-DCMAKE_TOOLCHAIN_FILE={}".format(toolchain_file))
    if generator:
        cmd.append("-G{}".format(generator))
    if tests:
        cmd.append("-DGG_ENABLE_UNIT_TESTS=1")
    if coverage:
        cmd.append("-DGG_COVERAGE=True")
    if sanitize:
        cmd.append("-DGG_SANITIZE=" + ",".join(sanitize))
    if docs:
        cmd.append("-DGG_GENERATE_DOCS=True")
    if cmake_extras:
        cmd.append(cmake_extras)
    ctx.run(" ".join(cmd))
