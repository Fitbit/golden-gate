import os

from invoke import task
from invoke.exceptions import Exit

from .. import xcodebuild
from .. import carthage
from ..utils import apple_build

@task(xcodebuild.check_version)
def build(ctx, configuration="Debug"):
    '''Build Golden Gate Host for iOS'''
    tmp_dir = ctx.C.APPLE_BUILD_TEMP_DIR

    # build Golden Gate iOS Host
    apple_build(ctx,
                scheme="GoldenGateHost-iOS",
                configuration=configuration,
                tmp_dir=tmp_dir)
