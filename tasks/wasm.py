# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Tasks to build Golden Gate for the Web Assembly platform"""
import os
from invoke import task, exceptions

from . import cmake

@task
def build(ctx):
    '''Build Golden Gate for the Web Assembly platform'''
    build_dir = os.path.join(ctx.C.BUILD_DIR, "wasm")
    cmake.build(ctx, build_dir, profile='wasm', cmake_wrapper='emcmake')
    ctx.run("cmake --build {build}".format(build=build_dir), pty=True)

@task
def clean(ctx):
    '''Blow away the build directory'''
    build_dir = os.path.join(ctx.C.BUILD_DIR, "wasm")
    ctx.run("rm -rf {}".format(build_dir))
