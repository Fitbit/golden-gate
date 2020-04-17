# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

from invoke import task

from .ios.xp import gen as ios_gen
from .macos.xp import gen as macos_gen

@task(ios_gen, macos_gen)
def gen(_ctx):
    "Generate all Xcode projects to build Golden Gate for Apple platforms"
    pass
