# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

"""Tasks to build the Golden Gate documentation"""

import os
import sys
import shutil
from invoke import task, exceptions, Collection
from . import cmake
from . import native
from . import android

@task(default=True)
def doxygen_build(ctx):
    '''Generate Golden Gate API documentation using doxygen'''
    build_dir = ctx.C.BUILD_DIR_NATIVE
    cmake.build(ctx, build_dir, native.detect_profile(), docs=True)
    ctx.run('cmake --build "{build}" --target doxyge'.format(build=build_dir), pty=(sys.platform != 'win32'))

doxygen = Collection('doxygen')
doxygen.add_task(doxygen_build, 'build')

@task(doxygen_build, android.documentation, default=True)
def mkdocs_build(ctx):
    '''Generate the Golden Gate documentation using MkDocs'''

    # First copy the javadoc
    dst_root_dir = os.path.join(ctx.C.DOC_DIR, 'src/api/javadoc')
    for module in ['GoldenGateBindings', 'GoldenGateConnectionManager', 'RxFitbitGatt']:
        src_dir = os.path.join(ctx.C.PLATFORM_DIR, 'android/goldengate/{}/build/javadoc'.format(module))
        dst_dir = os.path.join(dst_root_dir, module)
        if os.path.exists(dst_dir):
            print('Removing javadoc copy at {}'.format(dst_dir))
            shutil.rmtree(dst_dir)
        print('Copying javadoc for {}'.format(module))
        shutil.copytree(src_dir, os.path.join(dst_root_dir, module))

    # Build the HTML docs
    with ctx.cd(ctx.C.DOC_DIR):
        ctx.run("mkdocs build")

    print("HTML docs built, you can now open {}/site/index.html".format(ctx.C.DOC_DIR))

mkdocs = Collection('mkdocs')
mkdocs.add_task(mkdocs_build, 'build')

# Export a namespace
ns = Collection(doxygen, mkdocs)
