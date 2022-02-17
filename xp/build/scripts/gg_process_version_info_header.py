#!/usr/bin/env python

# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

#####################################################################
# Imports
#####################################################################
import os
import re
import subprocess
import sys
from os.path import relpath

# Generate absolute paths for needed dirs and scripts
scripts_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = os.path.join(scripts_dir, "..", "..", "..")

#####################################################################
def print_usage_and_exit():
    sys.stderr.write("""\
Usage: gg_version_info_gen.py <output_dir>
  Where <output_dir> is the parent folder where xp/common/gg_version_info.h
  will be written to
""")
    sys.exit(1)

#####################################################################
def print_error(error):
    sys.stderr.write("ERROR: %s\n" % (error))

def run_command(cmd):
    return subprocess.check_output(cmd,
                                   stderr=subprocess.STDOUT,
                                   cwd=root_dir,
                                   shell=True).strip()

def mkdir(path):
    try:
        os.makedirs(path)
    except OSError:
        if not os.path.isdir(path):
            raise

def readFile(path):
    try:
        return open(path, "r").read()
    except IOError:
        return []

def main():
    if len(sys.argv) != 2:
        print_error("ERROR: invalid/missing arguments")
        print_usage_and_exit()

    # Assign the parameters
    output_dir = sys.argv[1]

    # Verify it is a valid git repo (directory) or submodule (file)
    git_path = os.path.join(root_dir, ".git")
    if not os.path.exists(git_path):
        print_error("ERROR: not a valid git repo")
        return 1

    # Extract data from git
    git_hash = run_command("git log -1 --format=%h").decode('utf-8')
    num_commits = run_command("git log --oneline --first-parent | wc -l | tr -d \" \"").decode('utf-8')
    branch = run_command("git rev-parse HEAD | xargs git name-rev | cut -d' ' -f2").decode('utf-8')
    mods = subprocess.call("git diff-index --quiet HEAD", stderr=subprocess.STDOUT, shell=True)
    if mods:
        git_hash = "{}*".format(git_hash)

    print("Captured Git commit info: hash={} branch={} commits={}".format(git_hash, branch, num_commits))

    placeholder_map = {
        'GG_GIT_COMMIT_HASH': '{}'.format(git_hash),
        'GG_GIT_BRANCH_NAME': '{}'.format(branch),
        'GG_GIT_COMMIT_COUNT': '{}'.format(num_commits)
    }

    # Read the entire project file
    input_file = os.path.join(root_dir, "xp", "common", "gg_version_info.h.in")
    header = open(input_file, "r").read()

    def update_placeholders(match):
        placeholder = match.group(1)
        return placeholder_map.get(placeholder, placeholder)

    # Substitute placeholders
    header = re.sub(r'@([^@\n]+)@', update_placeholders, header)

    # Write to output
    output_file = os.path.join(output_dir, "xp", "common", "gg_version_info.h")
    print("Reading existing file @ path: {}".format(output_file))

    mkdir(os.path.dirname(output_file))
    existing_header = readFile(output_file)
    print("Existing header:\n {}".format(existing_header))

    if existing_header != header:
        print("writing new version to {}".format(output_file))
        open(output_file, "w+").write(header)


if __name__ == "__main__":
    sys.exit(main())
