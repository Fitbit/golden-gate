# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

import os

from invoke import task
from invoke.exceptions import Exit

from .. import xcodebuild
from .. import carthage
from ..utils import apple_build, apple_test

@task(xcodebuild.check_version)
def build(ctx, configuration="Debug"):
    '''Build Golden Gate for iOS'''
    apple_build(
        ctx,
        scheme="GoldenGate",
        configuration=configuration,
        destination=ctx.xcodebuild.destination)

@task(xcodebuild.check_version)
def test(ctx, coverage=False, skip_build=False):
    '''Tests GoldenGate Bindings for iOS'''
    apple_test(
        ctx,
        profile="iOS",
        destination=ctx.xcodebuild.destination,
        coverage=coverage,
        skip_build=skip_build)
