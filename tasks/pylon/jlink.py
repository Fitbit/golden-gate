"""JLink related tasks"""
import re
import socket

from functools import reduce  # pylint: disable=W0622

from invoke import task

from ..deps import version_check, build_instructions

@task
def check_version(ctx, abort=True):
    '''Check if JLinkExe is installed with a minimum version'''
    return version_check(
        ctx, "JLinkExe <<EOF\nexit\nEOF",
        min_version="6.0.0",
        on_fail=build_instructions(
            name="JLinkExe",
            link="https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack",
            abort=abort,
        )
    )

def run(ctx, sn, cmds, device="nRF52", speed="4000"):  # pylint: disable=C0103
    '''Connect to device using JLinkExe and execute list jlink of commands'''

    cmd_list = reduce(lambda x, y: x + "\n" + y, cmds) + "\nexit\n"
    cmd = "JLinkExe -device {device} -speed {speed} -if SWD -selectemubysn {sn} <<EOF\n{cmds}EOF"
    cmd = cmd.format(device=device, speed=speed, cmds=cmd_list, sn=sn)

    ctx.run(cmd)

def flash(ctx, file_path, sn, flash_addr, device="nRF52", speed="4000"):  # pylint: disable=C0103
    # JLinkExe requires the file to end with .bin
    ctx.run("cp \"{}\" \"{}.bin\"".format(file_path, file_path))

    run(ctx, sn, [
        "connect",
        "loadbin \"{fname}.bin\" {addr}".format(fname=file_path, addr=hex(flash_addr)),
        # Mynewt flash scripts suggest this is needed for flash not to fail
        "Regs"], device, speed)

    # Cleanup
    ctx.run("rm \"{}.bin\"".format(file_path))

def get_unused_ports(count):
    '''Get a number of unused ports'''
    sockets = []

    for i in range(count):
        sock = socket.socket()
        # Binding to port 0 will let the OS select an unused port
        sock.bind(('localhost', 0))
        sockets.append(sock)

    ports = [sock.getsockname()[1] for sock in sockets]

    # Close sockets because the ports became used after the call to bind
    for i in range(count):
        sockets[i].close()

    return ports

def get_emu_sn_list(ctx, product_name=None):
    '''Get list of emulator Serial Numbers, optionally filtered by Product Name'''
    cmds_list = "\nShowEmuList\nexit\n"
    cmd = "JLinkExe <<EOF{cmds}EOF".format(cmds=cmds_list)

    res = ctx.run(cmd)

    # Get output lines and keep only those form the ShowEmuList command with
    # a given product name (is specified)
    lines = str(res.stdout).split('\n')
    regex = re.compile(r'J-Link\[[0-9]+\]:*')

    if product_name:
        product_str = "ProductName: {name}".format(name=product_name)
    else:
        product_str = ''

    lines = [line for line in lines if regex.search(line) and product_str in line]

    # Extract S/N entry
    regex = re.compile(r'Serial number: [0-9]+')
    lines = [regex.search(line).group(0) for line in lines]

    # Extract only S/N
    regex = re.compile(r'[0-9]+')
    return [regex.search(line).group(0) for line in lines]

def connect_rtt(ctx, sn, reset_board=False, device="nRF52", speed="4000"):  # pylint: disable=C0103
    '''Connect to Segger RTT (Real Time Terminal)'''

    gdb_cmd = "JLinkGDBServer -device {device} -speed {speed} -if SWD {gdb_extra_args}"
    rtt_client_cmd = "JLinkRTTClient {rtt_client_extra_args}"

    if reset_board:
        # Reset board using J-Link with commands from generated script file
        reset_cmd = "echo -e \"r\ngo\nexit\n\" > {reset_script} && " + \
                    "JLinkExe -device {device} -speed {speed} -if SWD -autoconnect 1 " + \
                    "-CommanderScript {reset_script} {jlink_extra_args} && " + \
                    "rm {reset_script}"
    else:
        reset_cmd = "true"

    # Select specific device by S/N and random ports for GDB and RTT
    ports = get_unused_ports(2)

    gdb_extra_args = "-select usb={sn} -port {gdb_port} -RTTTelnetPort {rtt_port}"
    gdb_extra_args = gdb_extra_args.format(sn=sn, gdb_port=ports[0], rtt_port=ports[1])

    jlink_extra_args = "-selectemubysn {sn}".format(sn=sn)

    rtt_client_extra_args = "-LocalEcho Off -RTTTelnetPort {port}".format(port=ports[1])

    # Connect to RTT by starting GDB server in background, sleeping a few seconds
    # for it to start listening for connections, optionally issue a reset command,
    # and starting the RTT client
    cmd = gdb_cmd + " & sleep 2 && " + reset_cmd + " && " + rtt_client_cmd
    cmd = cmd.format(
        device=device,
        speed=speed,
        reset_script=".jlink_reset_script",
        gdb_extra_args=gdb_extra_args,
        jlink_extra_args=jlink_extra_args,
        rtt_client_extra_args=rtt_client_extra_args)

    ctx.run(cmd)

def connect_gdb(ctx, sn, port, elf_file=None, device="nRF52", speed="4000"):  # pylint: disable=C0103
    '''Connect to GDB server'''

    if port is None:
        # No port means that we should also start the GDB server
        port = get_unused_ports(1)[0]

        gdb_cmd = "JLinkGDBServer -device {device} -speed {speed} -if SWD {extra_args}"
        gdb_extra_args = "-select usb={sn} -port {port}".format(sn=sn, port=port)
        gdb_cmd = gdb_cmd.format(device=device, speed=speed, extra_args=gdb_extra_args)
    else:
        gdb_cmd = "true"

    cmd = "{gdb} & sleep 2 && arm-none-eabi-gdb {elf_file} -ex \"target remote localhost:{port}\""
    cmd = cmd.format(gdb=gdb_cmd, elf_file=elf_file, port=port)

    ctx.run(cmd)
