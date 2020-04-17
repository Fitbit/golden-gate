# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

#!/bin/bash

# Script used to generate GoldenGateXP modulemap dynamically

echo "Generating GoldenGateXP module map"

export SCRIPT_DIR=$(cd "$(dirname "$0")"; pwd)
echo $SCRIPT_DIR
cd $SCRIPT_DIR/../../..

# Create symbolic links in a directory named 'GoldenGateXP', so that the "header" directives in
# the modulemap file can be found relative to the location of that modulemap file
mkdir -p platform/apple/xp/generated/GoldenGateXP
ln -fhs xp/bundle/module.modulemap platform/apple/xp/generated/GoldenGateXP
for file in external/smo/c/lib/inc/*.h; do ln -fhs ../../../../../$file platform/apple/xp/generated/GoldenGateXP; done
ln -fhs ../../../../../xp platform/apple/xp/generated/GoldenGateXP/xp
