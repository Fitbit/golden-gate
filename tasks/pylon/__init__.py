# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Tasks to build for Pylon"""
import os

from invoke import Collection, task

from . import boards
from . import util

from . import  apps
from . import  jlink
from . import  newt
from . import  xp
from .apps.util import get_mem_usage
from .apps.util import print_mem_usage

@task
def clean(ctx):
    """Cleans Pylon/Newt folders"""
    if ctx.run("which newt", hide="both", warn=True):
        proj = ["platform/mynewt/apps/gg-tool"]
        for app_dir in proj:
            with ctx.cd(app_dir):
                if os.path.isdir(os.path.join(app_dir, "bin")):
                    ctx.run("newt clean all")

    ctx.run("rm -rf {}/pylon".format(ctx.C.BUILD_DIR))

@task(newt.check_version, jlink.check_version)
def provision(ctx, sn=None, board=None):  # pylint: disable=C0103
    '''Provision Pylon board for fist time use'''
    sn = util.get_device_sn(ctx, sn)
    if sn is None:
        return

    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    # Connect with JLink to erase internal flash
    jlink.run(ctx, sn, ["connect", "erase"])

    # Reconnect with JLink to enable reset button (need to do this in a
    # separate connection to avoid write after download warning)
    board_const = boards.get_board_constants(board_name)
    jlink.run(ctx, sn, [
        "connect",
        "w4 0x10001200 {}".format(hex(board_const["pselreset_reg_val"])),
        "w4 0x10001204 {}".format(hex(board_const["pselreset_reg_val"]))])

    # Need to build the GG XP lib first, as the gg-tool project has symbolic
    # links to it and the newt build command for the bootloader will fail
    # if those links point to non-existing files
    xp.build(ctx, board=board)

    # Build and flash Mynewt bootloader
    app_dir = "platform/mynewt/apps/gg-tool"
    with ctx.cd(app_dir):
        file_path = "{app_dir}/bin/targets/boot_{board}/app/boot/mynewt/mynewt.elf.bin"
        file_path = file_path.format(app_dir=app_dir, board=board_name)

        if not os.path.isdir(os.path.join(app_dir, "repos")):
            ctx.run("newt -v upgrade")
        ctx.run("newt build boot_{}".format(board_name))

    jlink.flash(ctx, file_path, sn, 0)

@task(jlink.check_version)
def install(ctx, file_path, sn=None, board=None):  # pylint: disable=C0103
    '''Flash image file to Pylon board'''
    sn = util.get_device_sn(ctx, sn)
    if sn is None:
        return

    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    # Flash app in first app slot
    board_const = boards.get_board_constants(board_name)
    jlink.flash(ctx, file_path, sn, board_const["flash_start_addr"])

@task(jlink.check_version)
def serial(ctx, sn=None, reset_board=False):  # pylint: disable=C0103
    '''Connect to Pylon board serial'''
    sn = util.get_device_sn(ctx, sn)
    if sn is None:
        return

    jlink.connect_rtt(ctx, sn=sn, reset_board=reset_board)

@task
def mem_usage(ctx, elf_file=None, img_file=None, board=None):
    """Print Pylon memory usage"""
    if elf_file and not os.path.exists(elf_file):
        print("File {} doesn't exist!".format(elf_file))
    elif img_file and not os.path.exists(img_file):
        print("File {} doesn't exist!".format(img_file))
    elif not img_file and not elf_file:
        print("Specify --elf-file and/or --img-file!")
    else:
        usage = get_mem_usage(ctx, elf_file, img_file, board)
        print_mem_usage(usage)

ns = Collection(clean, provision, install, serial, mem_usage)  # pylint: disable=C0103
ns.add_collection(apps)
ns.add_collection(jlink)
ns.add_collection(newt)
ns.add_collection(xp)

config = {  # pylint: disable=C0103
    "pylon": {
        "board": "nrf52840pdk"
    }
}
