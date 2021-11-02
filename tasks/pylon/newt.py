# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Newt related tasks"""
from invoke import task

from ..deps import version_check, build_instructions

@task
def check_version(ctx, min_version="1.9.0", abort=True):
    '''Check if Newt Tool is installed with a minimum version'''
    return version_check(
        ctx, "newt version",
        min_version=min_version,
        on_fail=build_instructions(
            name="Newt Tool",
            link="https://mynewt.apache.org/os/get_started/native_install_intro/",
            install_command=" && ".join([
                "brew tap juullabs-oss/mynewt",
                "brew update",
                "brew install mynewt-newt",
            ]),
            abort=abort,
        )
    )
