import os

from invoke import task

from .. import xcodebuild
from ... import cmake

@task
def gen(ctx):
    '''Generate iOS Xcode project for XP libs'''
    toolchain = os.path.join(os.getcwd(), "xp/config/toolchains/iOS.cmake")
    build_dir = os.path.join(ctx.C.BUILD_DIR, "xcode-iOS")
    cmake.build(ctx, build_dir, "iOS", toolchain_file=toolchain, generator="Xcode")

    # Generate GoldenGateXP modulemap
    script = os.path.join(ctx.C.BUILD_ROOT_DIR, "scripts", 'xcode-xp-framework-modulemap-gen.sh')
    ctx.run("%s" % (script))

@task(xcodebuild.check_version, gen)
def build(ctx, configuration="Debug"):
    '''Build Golden Gate XP for iOS'''
    build_dir = os.path.join(ctx.C.BUILD_DIR, "xcode-iOS")
    output_dir = os.path.join(build_dir, "output")

    xcodebuild.build(
        ctx,
        build_dir,
        project="golden-gate-ios.xcodeproj",
        scheme="GoldenGateXP",
        configuration=configuration,
        tmp_dir=output_dir,
        install_dir=output_dir)
