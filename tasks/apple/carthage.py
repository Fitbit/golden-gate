# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""
Carthage Tasks
"""
import os
import signal

from invoke import task
from invoke.exceptions import Exit
from tempfile import NamedTemporaryFile

from ..deps import version_check, build_instructions, run_and_extract_version
from . import xcodebuild

@task
def check_carthage_version(ctx, min_version="0.27.0", abort=True):
    '''Check if Carthage is installed with a minimum version'''
    return version_check(
        ctx, "carthage version",
        min_version=min_version,
        on_fail=build_instructions(
            name="Carthage",
            link="https://github.com/Carthage/Carthage",
            install_command="brew install carthage",
            abort=abort,
        )
    )

@task
def check_rome_version(ctx, min_version="0.13.0", abort=True):
    '''Check if Rome is installed with a minimum version'''
    return version_check(
        ctx, "rome --version",
        min_version=min_version,
        on_fail=build_instructions(
            name="Rome",
            link="https://github.com/blender/Rome",
            install_command="brew install blender/homebrew-tap/rome",
            reason="for faster builds",
            abort=abort,
        )
    )

@task
def _check_cwd(ctx):
    '''Check that we are invoked on the root directory.'''
    if ctx.C.ROOT_DIR != os.getcwd():
        print("Please run this command from the project's root.")
        raise Exit(-1)

def swift_version(ctx):
    '''Return the current Swift version'''
    version = run_and_extract_version(ctx, "xcrun swift -version")

    if not version:
        print("Couldn't determine Swift version.")
        raise Exit(-1)

    return version

def _rome_prefix(ctx):
    return "Swift_" + swift_version(ctx).replace('.', '_')

def _rome_list_cmd(ctx, missing, platform):
    cmd = "rome list \
           --platform {} \
           --cache-prefix {}".format(platform, _rome_prefix(ctx))
    return cmd if not missing else cmd + " --missing"

def _rome_download_cmd(ctx, platform):
    return "rome download \
            --platform {} \
            --cache-prefix {}".format(platform, _rome_prefix(ctx))

def _rome_upload_cmd(ctx, platform):
    return "rome upload \
            --platform {} \
            --cache-prefix {}".format(platform, _rome_prefix(ctx))

def _carthage_setup_environment():
    # Applying Carthage build workaround to exclude Apple Silicon binaries.
    # See https://github.com/Carthage/Carthage/issues/3019 for more details
    fp = NamedTemporaryFile(delete=False, prefix='static.xcconfig.')
    fp.write(b'EXCLUDED_ARCHS__EFFECTIVE_PLATFORM_SUFFIX_simulator__NATIVE_ARCH_64_BIT_x86_64__XCODE_1200 = arm64 arm64e armv7 armv7s armv6 armv8\n')
    fp.write(b'EXCLUDED_ARCHS = $(inherited) $(EXCLUDED_ARCHS__EFFECTIVE_PLATFORM_SUFFIX_$(EFFECTIVE_PLATFORM_SUFFIX)__NATIVE_ARCH_64_BIT_$(NATIVE_ARCH_64_BIT)__XCODE_$(XCODE_VERSION_MAJOR))\n')
    fp.write(b'ENABLE_TESTING_SEARCH_PATHS=YES\n')
    fp.write(b'OTHER_LDFLAGS=""\n')
    fp.close()

    handler = lambda a, b: os.unlink(fp.name)

    signal.signal(signal.SIGINT, handler)
    signal.signal(signal.SIGTERM, handler)
    signal.signal(signal.SIGHUP, handler)
    signal.signal(signal.SIGQUIT, handler)

    os.environ['XCODE_XCCONFIG_FILE'] = fp.name

def _carthage_bootstrap_cmd():
    _carthage_setup_environment()

    return "carthage bootstrap --platform ios,macos --cache-builds"

def _carthage_build_cmd(ctx):
    _carthage_setup_environment()

    # Carthage 0.29.0 moved the `--no-use-binaries` to the `build` command.
    if version_check(ctx, "carthage version", min_version="0.29.0"):
        return "carthage build --cache-builds --no-use-binaries"

    return "carthage build --cache-builds"

def _carthage_checkout_cmd(ctx):
    # Carthage 0.29.0 moved the `--no-use-binaries` to the `build` command.
    if version_check(ctx, "carthage version", min_version="0.29.0"):
        return "carthage checkout"

    return "carthage checkout --no-use-binaries"

def _workflow_cmd(ctx, name):
    return os.path.join(ctx.C.ROOT_DIR, "external/workflows", name)

@task(check_rome_version)
def rome_list(ctx, missing=False, platform="ios,macos"):
    '''Return the list of Carthage Frameworks needed'''
    ctx.run(_rome_list_cmd(ctx, missing, platform))

@task(check_rome_version)
def rome_download(ctx, platform="ios,macos"):
    '''Download Carthage Frameworks from Rome cache'''
    ctx.run(_rome_download_cmd(ctx, platform))

@task(check_rome_version)
def rome_upload(ctx, missing=False, platform="ios,macos"):
    '''Upload Carthage Frameworks to Rome cache'''
    cmd = _rome_upload_cmd(ctx, platform)

    if missing:
        cmd = " | ".join([
            _rome_list_cmd(ctx, missing, platform),
            "awk '{print $1}'",
            "xargs " + cmd])

    ctx.run(cmd)

def _build(ctx, missing=False, platform="ios,macos"):
    '''Build Carthage Frameworks missing from Rome cache'''
    cmd = _carthage_bootstrap_cmd()

    if missing:
        cmd = " | ".join([
            _rome_list_cmd(ctx, missing, platform),
            "awk '{print $1}'",
            "xargs " + cmd])

    ctx.run(cmd)

def _set_xcode_project_visibility(ctx, visible):
    if visible:
        ctx.run("ln -sf Checkouts Carthage/Dev")
    else:
        ctx.run("rm -f Carthage/Dev")

@task
def developer_checkouts(ctx):
    '''
    Setup symlinks to checkouts in the parent directory

    Useful if you are actively working on a dependency
    that you have checked out it in their parent folder.
    '''
    if not ctx.carthage.dev:
        print("Warning: `carthage.dev` was not enabled,")
        print("so Xcode will not use the sources.")
        print("Change it in `~/.invoke.yml` and re-run `bootstrap`.")
        raise Exit(-1)

    ctx.run(_workflow_cmd(ctx, "carthage-developer-checkouts"))
    _set_xcode_project_visibility(ctx, True)

@task
def developer_uncheckouts(ctx, warn=False):
    '''Undo `developer_checkouts`'''
    ctx.run(_workflow_cmd(ctx, "carthage-developer-uncheckouts"), warn=warn)

    if not ctx.carthage.dev:
        _set_xcode_project_visibility(ctx, False)

@task(xcodebuild.check_version, check_carthage_version, _check_cwd)
def _precheck(ctx):  # pylint: disable=W0613
    pass

@task(_precheck)
def bootstrap(ctx, platform="ios,macos"):
    '''
    Update Carthage Frameworks.
    If Rome is available, use it for faster builds.

    Set `carthage.dev` in `~/.invoke.yml` if you want to
    force building dependencies from sources (nothing pre-built).
    This allows you to modify and set breakpoints in dependencies.
    '''

    if ctx.carthage.dev:
        print("Using sources...")
        ctx.run("rm -rf Carthage/Build")
        ctx.run(_carthage_checkout_cmd(ctx))
        developer_checkouts(ctx)
    #elif check_rome_version(ctx, abort=False) and ctx.carthage.rome:
    #    print("Using pre-built libraries (cached by Rome)...")
    #    developer_uncheckouts(ctx, warn=True)  # might not be there yet
    #    rome_download(ctx, platform=platform)
    #    _build(ctx, missing=True)
    #    rome_upload(ctx, missing=True, platform=platform)
    else:
        print("Using pre-built libraries...")
        developer_uncheckouts(ctx, warn=True)  # might not be there yet
        ctx.run(_carthage_bootstrap_cmd())

    # FIXME: temporary hack to get around the fact that Rxbit isn't
    # available as a Git repo, so instead we bootstrap it manually here
    # instead of through a Cartfile reference
    with ctx.cd("{}/external/Rxbit".format(ctx.C.ROOT_DIR)):
        ctx.run(_carthage_bootstrap_cmd())

@task(_precheck)
def build(ctx, dependencies, platform="ios,macos", configuration="Release"):
    '''Build the specified target'''
    ctx.run("{} --platform {} --no-skip-current --configuration {} {}".format(
        _carthage_build_cmd(ctx),
        platform,
        configuration,
        " ".join(dependencies)
    ))

@task(_precheck)
def archive(ctx, frameworks):
    '''Export & zip a target for binary distribution'''
    ctx.run("carthage archive {}".format(" ".join(frameworks)))
