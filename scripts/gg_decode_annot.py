#!/usr/bin/env python

# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import os.path
import sys

# Generate absolute paths for needed dirs and scripts
scripts_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = os.path.join(scripts_dir, "..")
nanopb_dir = os.path.join(root_dir, "external", "nanopb")
protobuf_dir = os.path.join(root_dir, "external", "protobuf-debug-annotations")
decoder_script = os.path.join(protobuf_dir, "scripts", "decode_annot.py")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", help="annotations dump file")
    args = parser.parse_args()

    # Command for generating Python classes needed by decoder script
    make_cmd = "NANOPB_DIR={} make -C {} 1>/dev/null"
    make_cmd = make_cmd.format(nanopb_dir, protobuf_dir)

    # Command for running decoder script
    decode_cmd = "python {} --nanopb-dir={} --protobuf-dir={} {}"
    decode_cmd = decode_cmd.format(decoder_script, nanopb_dir, protobuf_dir, args.filename)

    rc = os.system(make_cmd)
    if rc:
        return rc

    os.system(decode_cmd)


if __name__ == "__main__":
    sys.exit(main())
