"""Helpers to check dependencies"""

from builtins import range
import re
import semver

from invoke import Exit

def _make_semver_version(version):
    parts = version.split(".")

    for _ in range(len(parts), 3):
        parts.append("0")

    return ".".join(parts[:3])

def run_and_extract_version(ctx, command):
    """Runs a command and extracts the first version number"""
    result = ctx.run(command, hide=True, warn=True)

    if not result:
        return None

    version = re.search(r'(\d+)?(\.\d+)?(\.\d+)', result.stdout)
    return _make_semver_version(version.group(0)) if version else None

def version_check(ctx, command, min_version=None, on_fail=None):
    '''Extract version from command and verify it meets the minimum requirement if specified'''
    version = run_and_extract_version(ctx, command)

    if not version:
        match = False
    elif min_version:
        match = semver.compare(version, _make_semver_version(min_version)) >= 0
    else:
        match = True

    if not match and on_fail:
        on_fail(version, min_version)

    return match

def build_instructions(name, link=None, install_command=None, reason="required", abort=True):
    """Helper to create `on_fail` handler for `version_check`"""
    def _on_fail(version, min_version):
        if version:
            print("{} {} doesn't satisfy requirements.".format(name, version))
        else:
            print("Couldn't find {}.".format(name))

        if min_version:
            print("Please install {} {} or later ({}).".format(name, min_version, reason))
        else:
            print("Please install {} ({}).".format(name, reason))

        print()

        if link:
            print("> {}".format(link))
            print()

        if install_command:
            print("$ {}".format(install_command))
            print()

        if abort:
            raise Exit(-1)

    return _on_fail
