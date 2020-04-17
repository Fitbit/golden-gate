# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

import os
import re
from invoke import task

@task
def build(ctx):
    '''Build a docker image that can be used to build the project'''
    command = "docker image build -t golden-gate:latest -f Dockerfile ."
    ctx.run(command)

@task
def run(ctx):
    '''Run a shell in the docker image configured for building the project'''
    command = "docker run --rm -it -v `pwd`:/home/gg/project -w /home/gg/project golden-gate bash"
    ctx.run(command, pty=True)
