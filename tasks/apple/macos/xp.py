import os

from invoke import task

from .. import xcodebuild
from ... import cmake

@task
def gen(ctx):
    '''Generate macOS Xcode project for XP libs'''
    build_dir = os.path.join(ctx.C.BUILD_DIR, "xcode-macOS")
    cmake.build(ctx, build_dir, "macOS", generator="Xcode")

    # Generate GoldenGateXP modulemap
    script = os.path.join(ctx.C.BUILD_ROOT_DIR, "scripts", 'xcode-xp-framework-modulemap-gen.sh')
    ctx.run("%s" % (script))

@task(xcodebuild.check_version, gen)
def build(ctx, configuration="Debug"):
    '''Build Golden Gate for Mac OS'''
    build_dir = os.path.join(ctx.C.BUILD_DIR, "xcode-macOS")
    output_dir = os.path.join(build_dir, "output")

    xcodebuild.build(
        ctx,
        build_dir,
        project="golden-gate-macos.xcodeproj",
        scheme="GoldenGateXP",
        configuration=configuration,
        tmp_dir=output_dir,
        install_dir=output_dir)
