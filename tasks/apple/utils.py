# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

import os

from . import xcodebuild
from .coverage import coverage as apple_coverage

def apple_build(
        ctx,
        scheme,
        configuration="Debug",
        destination=None,
        tmp_dir=None,
        archive_path=None,
        prefix=None):
    '''Build iOS or macOS scheme'''
    tmp_dir = tmp_dir or ctx.C.APPLE_BUILD_TEMP_DIR
    build_dir = os.path.join(ctx.C.PLATFORM_DIR, "apple")
    xcodebuild.build(
        ctx,
        build_dir,
        workspace="GoldenGate.xcworkspace",
        scheme=scheme,
        configuration=configuration,
        destination=destination,
        install_dir=tmp_dir,
        tmp_dir=tmp_dir,
        archive_path=archive_path,
        prefix=prefix,
        postfix="EXTRA_SWIFTLINT_PARAMS={}".format(os.environ.get('EXTRA_SWIFTLINT_PARAMS')))

def apple_test(ctx, profile, destination=None, coverage=False, skip_build=False):
    '''Test iOS or macOS GoldenGate framework'''
    tmp_dir = ctx.C.APPLE_BUILD_TEMP_DIR
    build_dir = os.path.join(ctx.C.PLATFORM_DIR, "apple")
    xcodebuild.test(
        ctx,
        workspace="GoldenGate.xcworkspace",
        scheme="GoldenGate",
        destination=destination,
        build_dir=build_dir,
        tmp_dir=tmp_dir,
        coverage=coverage,
        skip_build=skip_build)

    if coverage:
        apple_coverage(
            ctx,
            build_dir,
            tmp_dir,
            scheme="GoldenGate",
            profile=profile)
