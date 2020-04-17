# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

from invoke import task
from invoke.exceptions import Exit

from ..deps import version_check, build_instructions

def test(
        ctx,
        workspace,
        scheme,
        build_dir,
        tmp_dir,
        destination=None,
        configuration="Debug",
        skip_build=False,
        coverage=False):

    cmd = ["set -o pipefail && xcrun", "xcodebuild"]

    def add_argument(name, value, replacement=None):
        if value:
            cmd.extend(["-{}".format(name), replacement if replacement else repr(str(value))])

    add_argument("workspace", workspace)
    add_argument("scheme", scheme)
    add_argument("configuration", configuration)
    add_argument("derivedDataPath", tmp_dir)
    add_argument("enableCodeCoverage", coverage, "YES")
    if destination:
        add_argument("destination", destination)

    if tmp_dir:
        cmd.append("DSTROOT='{}'".format(tmp_dir))

    cmd.append("test-without-building" if skip_build else "test")

    # Sadly xcpretty swallows information on crashes/interrupts/assertions.
    # https://github.com/supermarin/xcpretty/pull/240#issuecomment-366874845
    # So, only use xcpretty if the developer made an explicit opt-in.
    if ctx.xcodebuild.xcpretty_for_tests:
        if ctx.run("bundle show xcpretty", pty=True, hide="out", warn=True):
            cmd.append("| bundle exec xcpretty")
        elif ctx.run("which xcpretty", pty=True, hide="out", warn=True):
            cmd.append("| xcpretty")

    with ctx.cd(build_dir):
        cmd = " ".join(cmd)
        print("{}".format(cmd))
        ctx.run(cmd)

def build(
        ctx,
        build_dir,
        destination=None,
        target=None,
        scheme=None,
        configuration=None,
        tmp_dir=None,
        project=None,
        workspace=None,
        install_dir=None,
        archive_path=None,
        prefix=None,
        postfix=None):
    '''Build the xp Xcode project using xcodebuild'''

    cmd = ["set -o pipefail && xcrun", "xcodebuild"]

    # prepend overrides such as "CODE_SIGN_IDENTITY= CODE_SIGNING_REQUIRED=NO"
    if prefix:
        cmd.append(prefix)

    # increase concurrent compile tasks number (set to CPU number).
    cmd.append("-IDEBuildOperationMaxNumberOfConcurrentCompileTasks=`sysctl -n hw.ncpu`")

    if project:
        cmd.append("-project '{}'".format(project))
    elif workspace:
        cmd.append("-workspace '{}'".format(workspace))
    else:
        raise Exit(-1)

    if scheme:
        cmd.append("-scheme '{}'".format(scheme))

        # DerivedData path can only be specified for schemes or tests
        if tmp_dir:
            cmd.append("-derivedDataPath '{}'".format(tmp_dir))
    elif target:
        cmd.append("-target '{}'".format(target))
    else:
        print("No scheme or target specified!")
        raise Exit(-1)

    if destination:
        cmd.append("-destination '{}'".format(destination))

    if configuration:
        cmd.append("-configuration '{}'".format(configuration))

    if archive_path:
        cmd.append("-archivePath '{}' archive".format(archive_path))
    elif install_dir:
        cmd.append("DSTROOT='{}' install".format(install_dir))

    if postfix:
        cmd.append(postfix)

    if ctx.xcodebuild.xcpretty_for_builds:
        if ctx.run("bundle show xcpretty", pty=True, hide="out", warn=True):
            cmd.append("| bundle exec xcpretty")
        elif ctx.run("which xcpretty", pty=True, hide="out", warn=True):
            cmd.append("| xcpretty")

    with ctx.cd(build_dir):
        cmd = " ".join(cmd)
        print("{}".format(cmd))
        ctx.run(cmd)

@task
def check_version(ctx, min_version="9.0.1", abort=True):
    '''Check if Xcode is installed with a minimum version'''
    return version_check(
        ctx, "xcodebuild -version | head -1",
        min_version=min_version,
        on_fail=build_instructions(
            name="Xcode",
            link="https://itunes.apple.com/us/app/xcode/id497799835",
            abort=abort,
        )
    )
