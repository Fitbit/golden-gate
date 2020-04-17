# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

def coverage(
        ctx,
        project,
        binary_file,
        build_directory,
        output_directory,
        scheme,
        source_directory,
        source_files,
        workspace,
        configuration="Debug",
        input_format="profdata",
        ignore=None,
        output_format="simple-output"):

    cmd = ["bundle", "exec", "slather", "coverage"]

    def add_argument(name, value):
        if value:
            cmd.extend(["--{}".format(name), repr(str(value))])

    add_argument("binary-file", binary_file)
    add_argument("build-directory", build_directory)
    add_argument("configuration", configuration)
    add_argument("input-format", input_format)
    add_argument("output-directory", output_directory)
    add_argument("scheme", scheme)
    add_argument("source-directory", source_directory)
    add_argument("source-files", source_files)
    add_argument("workspace", workspace)

    if ignore:
        for pattern in ignore:
            add_argument("ignore", pattern)

    # enforce strict checking here since we append pure flags
    if output_format in ["simple-output", "gutter-json", "cobertura-xml", "json", "html"]:
        cmd.append("--{}".format(output_format))
    else:
        raise Exception("Unknown output format: {}".format(output_format))

    cmd.append(repr(project))
    print(" ".join(cmd))
    ctx.run(" ".join(cmd))
