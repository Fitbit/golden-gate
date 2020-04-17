# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Tasks for building GG-Central and GG-Peripheral apps for Pylon"""
from invoke import task

from . import util

@task(pre=util.build.pre)
def build(ctx, export_path=None, board=None):
    """Build gg_central and gg_peripheral for Pylon"""

    if board is None:
        board_name = ctx.pylon.board
    else:
        board_name = board

    util.build(ctx, "gg_central", "platform/mynewt/apps/gg-tool", export_path, board_name)
    util.build(ctx, "gg_peripheral", "platform/mynewt/apps/gg-tool", export_path, board_name)
