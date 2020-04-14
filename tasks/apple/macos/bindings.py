from invoke import task

from .. import xcodebuild
from ..utils import apple_build, apple_test

@task(xcodebuild.check_version)
def build(ctx, configuration="Debug"):
    '''Build Golden Gate for Mac OS'''
    apple_build(
        ctx,
        scheme="GoldenGate",
        configuration=configuration)

@task(xcodebuild.check_version)
def test(ctx, coverage=False, skip_build=False):
    '''Test Golden Gate for macOS'''
    apple_test(
        ctx,
        profile="macOS",
        coverage=coverage,
        skip_build=skip_build)
