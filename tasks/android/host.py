# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"Android Host Tasks"
import os

from invoke import task
from invoke.exceptions import Exit

from . import library

@task(library.build)
def build(ctx):
    '''Build Golden Gate host app for Android'''
    with ctx.cd(os.path.join(ctx.C.ROOT_DIR, "platform", "android",
                             "goldengate")):
        ctx.run("./gradlew assembleDebug", pty=True)


@task(library.build, build)
def install(ctx):
    '''Installs .apk via ADB after building image'''
    host_apk = os.path.join(
        ctx.C.PLATFORM_DIR,
        "android", "goldengate", "app",
        "build", "outputs", "apk", "debug", "app-debug.apk")
    installfrompath(ctx, host_apk)

@task
def installfrompath(ctx, apk_path, device_udid=None):
    '''Installs .apk via ADB from the specified path'''
    # check lib adb is installed
    if ctx.run("which adb", pty=True, hide="both", warn=True).failed:
        print("Please install adb.")
        print("$ brew install adb")
        raise Exit(-1)

    # generate launch command
    cmd = ["adb install"]
    if device_udid:
        cmd.append("-s {}".format(device_udid))

    cmd.append("-r {}".format(apk_path))

    # execute command
    ctx.run(" ".join(cmd), echo=True)
