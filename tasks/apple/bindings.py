from invoke import task

from . import carthage
from .ios.bindings import build as ios_build
from .macos.bindings import build as macos_build

@task
def build(ctx, configuration="Debug"):
    '''Build ios and macos GoldenGate framework'''
    ios_build(ctx, configuration)
    macos_build(ctx, configuration)

@task(carthage._precheck)  # pylint: disable=W0212
def archive(ctx, platform="ios", build=True):  # pylint: disable=W0621
    '''Archive GoldenGate Framework for binary distribution'''
    framework = "GoldenGate"

    if build:
        carthage.build(ctx, [framework], platform=platform)

    carthage.archive(ctx, [framework])

    if build:
        # Remove built binaries to prevent them from being rsynced
        # by platform/apple/scripts/carthage-copy-and-verify
        ctx.run("rm -rf Carthage/Build/iOS/{}*".format(framework))
        ctx.run("rm -rf Carthage/Build/Mac/{}*".format(framework))
