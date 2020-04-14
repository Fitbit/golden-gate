"""
Clean up tasks
"""

from invoke import task

from . import android
from . import apple
from . import pylon

@task(name="all", default=True)
def all_(ctx):
    '''Blow away the build and platform build specific directories'''
    ctx.run("rm -rf {}".format(ctx.C.BUILD_DIR))

    # Clean more specific targets
    android.clean(ctx)
    apple.clean(ctx)
    pylon.clean(ctx)
