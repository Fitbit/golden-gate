# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Tasks for building GoldenGate for Pylon"""
import os
import sys

from invoke import task

from .. import cmake

@task
def build(ctx, profile="pylon/pylon", board=None):
    '''Build Golden Gate XP libs for the Pylon platform'''
    build_dir = os.path.join(ctx.C.BUILD_DIR, "pylon")

    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    profile = "pylon/{board}".format(board=board_name)

    cmake.build(ctx, build_dir, profile,
                toolchain_file=os.path.join(ctx.C.ROOT_DIR,
                                            "xp/config/toolchains/mynewt.cmake"))
    ctx.run("cmake --build {build}".format(build=build_dir), pty=(sys.platform != 'win32'))
