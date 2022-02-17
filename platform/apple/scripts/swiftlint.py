#!/usr/bin/env python

# pylint: disable=W0703

"""Helpers to check dependencies"""
from __future__ import print_function, absolute_import

import re
import subprocess
import sys

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

def print_usage_and_exit():
    sys.stderr.write("""\
Usage: swiftlint.py {--strict-version-check} --config <swiftlint-config-path>
  Where <swiftlint-config-path> is the path to swiftlint config file,
  and --strict-version-check will force this script to exit with an error
  if the swiftlint version doesn't meet the minimum requirements.
""")
    sys.exit(1)

def make_semver_version(version):
    parts = version.split(".")

    for _ in range(len(parts), 3):
        parts.append("0")

    return ".".join(parts[:3])

def run_and_extract_version(command):
    """Runs a command and extracts the first version number"""
    result = None

    try:
        result = subprocess.check_output(command, stderr=subprocess.STDOUT, shell=True).strip()
    except Exception:
        return None

    if not result:
        return None

    version = re.search(r'(\d+)?(\.\d+)?(\.\d+)', result)
    return make_semver_version(version.group(0)) if version else None

# quick and dirty version checking since semver module is not available by default python env
def version_compare(version, min_version):
    version = version.split(".")
    min_version = min_version.split(".")

    if not version or len(version) != len(min_version):
        raise "invalid version parameters " + version

    if version[0] > min_version[0]:
        return 1
    if version[0] < min_version[0]:
        return -1
    if len(version) > 1:
        return version_compare(".".join(version[1:]), ".".join(min_version[1:]))

    return 0

def version_check(command, min_version=None, on_fail=None):
    '''Extract version from command and verify it meets the minimum requirement if specified'''
    version = run_and_extract_version(command)

    if not version:
        match = False
    elif min_version:
        match = version_compare(version, make_semver_version(min_version)) >= 0
    else:
        match = True

    if not match and on_fail:
        on_fail(version, min_version)

    return match

def build_instructions(name, link=None, install_command=None, reason="required"):
    """Helper to create `on_fail` handler for `version_check`"""
    def _on_fail(version, min_version):
        if version:
            eprint("{} {} doesn't satisfy requirements.".format(name, version))
        else:
            eprint("Couldn't find {}.".format(name))

        if min_version:
            eprint("Please install {} {} or later ({}).".format(name, min_version, reason))
        else:
            eprint("Please install {} ({}).".format(name, reason))

        eprint()

        if link:
            eprint("> {}".format(link))
            eprint()

        if install_command:
            eprint("$ {}".format(install_command))
            eprint()

    return _on_fail

# version 0.29.1 is the first version that enforced the swiftlint_version rule
# however our use case requires that this enforcement be optional so we check
# the minimum version requirements manually instead.
def check_swiftlint_version(min_version="0.30.1"):
    '''Check if Swiftlint is installed with a minimum version'''
    return version_check(
        "swiftlint version",
        min_version=min_version,
        on_fail=build_instructions(
            name="Swiftlint",
            link="https://github.com/realm/SwiftLint",
            install_command="brew install swiftlint"
        )
    )

def main():
    sys.argv.reverse()
    sys.argv.pop()
    strict = False
    config_path = None
    while sys.argv:
        arg = sys.argv.pop()
        if arg == '--strict-version-check':
            strict = True
        elif not config_path and arg == '--config':
            arg = sys.argv.pop()
            config_path = arg
        else:
            eprint("unexpected argument " + arg)
            print_usage_and_exit()

    if not check_swiftlint_version():
        # if swiflint is missing or version is too old, fail silently
        # unless strict mode is specified
        return 1 if strict else 0

    if not config_path:
        eprint("invalid arguments")
        print_usage_and_exit()

    return subprocess.call("swiftlint --config {}".format(config_path), shell=True)


if __name__ == "__main__":
    sys.exit(main())
