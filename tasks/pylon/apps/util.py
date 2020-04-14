"""Helpers to build app-specific tasks"""
import os

from invoke import task

from .. import boards
from .. import jlink
from .. import newt
from .. import util
from .. import xp


def app_tasks(name, path):
    """Utility to create build and run tasks for Pylon apps"""
    @task(pre=reset_project.pre, name="reset_project")
    def _reset_project(ctx):
        reset_project(ctx, path)

    _reset_project.__doc__ = "Reset Mynewt project files for {}".format(name)

    @task(pre=install_project.pre, name="install_project")
    def _install_project(ctx):
        install_project(ctx, path)

    _install_project.__doc__ = "Install Mynewt project dependencies for {}".format(name)

    @task(pre=upgrade_project.pre, name="upgrade_project")
    def _upgrade_project(ctx):
        upgrade_project(ctx, path)

    _upgrade_project.__doc__ = "Upgrade Mynewt project dependencies for {}".format(name)

    @task(pre=build.pre, name="build")
    def _build(ctx, export_path=None, board=None):
        build(ctx, name, path, export_path, board)

    _build.__doc__ = "Build {} for Pylon".format(name)

    @task(pre=run.pre, name="run")
    def _run(ctx, sn=None, board=None):  # pylint: disable=C0103
        run(ctx, name, path, sn, board)

    _run.__doc__ = "Flash and run {} on Pylon".format(name)

    @task(pre=debug.pre, name="debug")
    def _debug(ctx, sn=None, port=None, board=None):  # pylint: disable=C0103
        debug(ctx, name, path, sn, port, board)

    _debug.__doc__ = "Debug {} on Pylon".format(name)

    return _install_project, _reset_project, _upgrade_project, _build, _run, _debug

@task(newt.check_version)
def install_project(ctx, path):
    """Install Mynewt project files"""
    with ctx.cd(path):
        ctx.run("newt -v install")

@task(newt.check_version)
def reset_project(ctx, path):
    """Reset Mynewt project files in case they got in an invalid state"""
    with ctx.cd(path):
        ctx.run("rm -rf project.state repos")
        ctx.run("newt -v install")

@task(newt.check_version)
def upgrade_project(ctx, path):
    """Upgrade Mynewt project dependencies (needed if project.yml changes)"""
    with ctx.cd(path):
        ctx.run("newt upgrade")

def get_mem_usage(ctx, elf_file=None, img_file=None, board=None):  # pylint: disable=R0914
    """Get Pylon memory usage"""

    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    ret = dict()
    board_const = boards.get_board_constants(board_name)
    boot_trailer_size = 16 + 128 * 3 + 1 + 1

    ram_size = board_const["ram_end_addr"] - board_const["ram_start_addr"]
    flash_size = board_const["flash_end_addr"] - board_const["flash_start_addr"]
    app_max_size = board_const["app_max_size"]

    if img_file is not None and os.path.exists(img_file):
        ret["App slot"] = dict()
        app_usage = os.stat(img_file).st_size
        ret["App slot"]["used"] = app_usage
        # Max app size = nRF52 app slot size - image boot trailer
        ret["App slot"]["free"] = app_max_size - boot_trailer_size - app_usage

    if elf_file is None or not os.path.exists(elf_file):
        return ret

    ram_sections = [".vector_relocation", ".data", ".bssnz", ".bss", ".stack_dummy"]
    flash_sections = [".imghdr", ".text", ".ARM.extab", ".ARM.exidx", ".data"]

    res = ctx.run("arm-none-eabi-size -A {}".format(elf_file), hide=True)
    lines = res.stdout.split('\n')
    lines = lines[2:len(lines) - 4]
    words = [line.split() for line in lines]

    ram_usage = 0
    flash_usage = 0

    for i in range(len(lines)):
        if words[i][0] in ram_sections:
            ram_usage = ram_usage + int(words[i][1])
        if words[i][0] in flash_sections:
            flash_usage = flash_usage + int(words[i][1])

    ret["RAM"] = dict()
    ret["RAM"]["used"] = ram_usage
    ret["RAM"]["free"] = ram_size - ram_usage

    ret["FLASH"] = dict()
    ret["FLASH"]["used"] = flash_usage
    ret["FLASH"]["free"] = flash_size - flash_usage

    return ret

def print_mem_usage(usage):
    """Print Pylon memory usage"""
    for region in usage.keys():
        used = usage[region]["used"]
        free = usage[region]["free"]
        usage_msg = "{region}:\n  used: {used} bytes\n  free: {free} bytes"
        usage_msg = usage_msg.format(region=region, used=used, free=free)
        print(usage_msg)

@task(newt.check_version)
def build(ctx, name, path, export_path=None, board=None):
    """Build an app at the specified path"""
    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    base_fname = "{path}/bin/targets/{app}_{board}/app/apps/{app}/{app}".format(
        path=path, app=name, board=board_name)
    elf_file = base_fname + ".elf"
    img_file = base_fname + ".img"

    old_usage = get_mem_usage(ctx, elf_file, img_file)

    xp.build(ctx, board=board)

    with ctx.cd(path):
        # Install dependencies if they are not present
        if not os.path.isdir(os.path.join(path, "repos/apache-mynewt-core")):
            # NOTE: here we continue even if there are errors, because of a bug
            # in newt 1.7 when performing a fresh install, which reports an error
            # even when it succeeds
            ctx.run("newt -v install", warn=True)
        ctx.run("newt build {app}_{board}".format(app=name, board=board_name))
        ctx.run("newt create-image {app}_{board} 1.0.0".format(app=name, board=board_name))

    if export_path:
        print("Exporting {app}.img to {export}".format(app=name, export=export_path))

        if not os.path.isdir(export_path):
            os.makedirs(export_path)

        ctx.run("cp {img} {export}".format(img=img_file, export=export_path))

    # Print image size statistics
    new_usage = get_mem_usage(ctx, elf_file, img_file, board_name)

    print_mem_usage(new_usage)

    for region in old_usage.keys():
        diff = new_usage[region]["used"] - old_usage[region]["used"]
        if diff > 0:
            diff_msg = "{region} usage increased by {diff} bytes since last build"
            print(diff_msg.format(region=region, diff=diff))
        elif diff < 0:
            diff_msg = "{region} usage decreased by {diff} bytes since last build"
            print(diff_msg.format(region=region, diff=-diff))

@task(newt.check_version, jlink.check_version)
def run(ctx, name, path, sn=None, board=None):
    """Flash and run an app at the specified path"""
    sn = util.get_device_sn(ctx, sn)
    if sn is None:
        return

    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    xp.build(ctx, board=board)

    with ctx.cd(path):
        if not os.path.isdir(os.path.join(path, "repos")):
            ctx.run("newt -v install")
        ctx.run("newt create-image {app}_{board} 1.0.0".format(app=name, board=board_name))

    img = "{path}/bin/targets/{app}_{board}/app/apps/{app}/{app}.img"
    img = img.format(path=path, app=name, board=board_name)

    # Flash app in first app slot
    board_const = boards.get_board_constants(board_name)
    jlink.flash(ctx, img, sn, board_const["flash_start_addr"])

@task(newt.check_version, jlink.check_version)
def debug(ctx, name, path, sn=None, port=None, board=None):
    """Connect debugger to Pylon"""
    sn = util.get_device_sn(ctx, sn)
    if sn is None:
        return

    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    elf_file = "{path}/bin/targets/{app}_{board}/app/apps/{app}/{app}.elf"
    elf_file = elf_file.format(path=path, app=name, board=board_name)

    jlink.connect_gdb(ctx, sn, port, elf_file)
