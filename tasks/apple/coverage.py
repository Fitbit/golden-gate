# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

import os

from . import slather

def coverage(ctx, build_dir, tmp_dir, scheme, profile):
    profile = {k.lower(): k for k in ["iOS", "macOS"]}[profile.lower()]
    variant_dir = {
        "iOS": "Debug-iphonesimulator",
        "macOS": "Debug"}[profile]
    binary_dir = "Build/Products/{}".format(variant_dir)

    with ctx.cd(os.path.join(build_dir, "bindings")):
        common = dict(
            binary_file=os.path.join(
                tmp_dir, binary_dir, "GoldenGate.framework/GoldenGate"),
            build_directory=tmp_dir,
            scheme=scheme,
            source_directory="GoldenGate",
            source_files="*.swift",
            workspace="../GoldenGate.xcworkspace",
            ignore=[
                "*Spec.swift",
                "Mock*.swift",
                "Tests/**/*",
                "../../../xp/**/*",
                "../../../external/**/*",
            ]
        )

        slather.coverage(
            ctx, "GoldenGate.xcodeproj",
            **dict(
                output_directory="../../../coverage/apple/html",
                output_format="html",
                **common
            )
        )

        slather.coverage(
            ctx, "GoldenGate.xcodeproj",
            **dict(
                output_directory="../../../coverage/apple",
                output_format="cobertura-xml",
                **common
            )
        )
