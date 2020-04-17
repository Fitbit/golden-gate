# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Tasks to build apps for Pylon"""
from invoke import Collection

from . import gg_tool
from . import  util

def _make_collection(name, path):
    coll = Collection(name)

    for task in util.app_tasks(name, path):
        coll.add_task(task)

    return coll

ns = Collection()  # pylint: disable=C0103
ns.add_collection(_make_collection("gg_central", "platform/mynewt/apps/gg-tool"))
ns.add_collection(_make_collection("gg_peripheral", "platform/mynewt/apps/gg-tool"))
ns.add_collection(gg_tool)
ns.add_collection(util)
