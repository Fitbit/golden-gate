import os

from invoke import task

from ..utils import apple_build
from .. import xcodebuild

def extract(ctx, tmp_dir, output_dir):
    '''Copy binary and dependencies into ./platform/apple/build/apps/host'''
    built_bundle = "{}/Applications/GoldenGateHost-macOS.app".format(tmp_dir)
    goldengate_frameworks = "{}/Contents/Frameworks/*.framework".format(built_bundle)
    goldengate_dylibs = "{}/Contents/Frameworks/*.dylib".format(built_bundle)
    goldengate_executable = "{}/Contents/MacOS/GoldenGateHost-macOS".format(built_bundle)
    with ctx.cd(output_dir):
        ctx.run("cp {} .".format(goldengate_executable))
        ctx.run("rsync -a --delete {} .".format(" ".join([
            goldengate_frameworks,
            goldengate_dylibs
        ])))

@task(xcodebuild.check_version)
def build(ctx, configuration="Debug"):
    '''Build Golden Gate Host for Mac OS'''
    tmp_dir = ctx.C.APPLE_BUILD_TEMP_DIR

    apple_build(ctx,
                scheme="GoldenGateHost-macOS",
                configuration=configuration,
                tmp_dir=tmp_dir)

    # extract Golden Gate Host so it can be executed from CLI
    output_dir = os.path.join(ctx.C.PLATFORM_DIR, "apple/build/apps/host")
    ctx.run("mkdir -p {}".format(output_dir))
    extract(ctx, tmp_dir=tmp_dir, output_dir=output_dir)

@task
def run(ctx):
    '''Run Golden Gate host for Mac OS'''
    bin_dir = os.path.join(ctx.C.PLATFORM_DIR, "apple/build/apps/host")
    with ctx.cd(bin_dir):
        ctx.run("./GoldenGateHost-macOS", pty=True)
