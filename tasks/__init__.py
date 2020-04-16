"""
Invoke configuration for Golden Gate
"""

# First check that we are running in a Python >= 3.5 environment
from __future__ import print_function
import sys
if not sys.version_info.major == 3 and sys.version_info.minor >= 5:
    print(
"""You are using 'invoke' in a Python 2.x environment, but Python >= 3.5 is required.
You have probably not activated the 'gg' conda environment, please check the 'Getting Started'
guide for more details on how to setup your environment""")
    sys.exit(1)

# Imports
import os
import subprocess

from invoke import Collection, Config, task

from . import android
from . import apple
from . import pylon
from . import native
from . import clean
from . import wasm
from . import doc
from . import docker

# Assuming you haven't moved the default location of '.git', the .git/ folder (even for submodules)
# will be at the root of the repo. Thus, find the folder .git/ is within and assume that's the root
GIT_DIR = subprocess.check_output("git rev-parse --show-toplevel",
                                  shell=True).strip().decode("utf-8")
ROOT_DIR = GIT_DIR

# Initialize constants that are common among all platforms/products
def initialize_constants(cfg):
    cfg.C = {}

    # We can't access the paths variable by using dot notation, since there is a paths() function
    # on a Config object. We much use Dictionary syntax.
    # http://docs.pyinvoke.org/en/0.15.0/api/config.html#module-invoke.config
    cfg.C.ROOT_DIR             = ROOT_DIR
    cfg.C.BIN_DIR              = os.path.join(cfg.C.ROOT_DIR, "bin")
    cfg.C.BUILD_ROOT_DIR       = os.path.join(cfg.C.ROOT_DIR, "xp/build")
    cfg.C.BUILD_DIR            = os.path.join(cfg.C.ROOT_DIR, "xp/build/cmake")
    cfg.C.BUILD_DIR_NATIVE     = os.path.join(cfg.C.BUILD_DIR, "native")
    cfg.C.PLATFORM_DIR         = os.path.join(cfg.C.ROOT_DIR, "platform")
    cfg.C.APPS_DIR             = os.path.join(cfg.C.BUILD_DIR_NATIVE, "apps")
    cfg.C.APPLE_BUILD_TEMP_DIR = os.path.join(cfg.C.PLATFORM_DIR, "apple/output")
    cfg.C.DOC_DIR              = os.path.join(cfg.C.ROOT_DIR, "docs")

config = Config(project_location=ROOT_DIR)
initialize_constants(config)

# Add collections
ns = Collection()
ns.add_collection(android)
ns.add_collection(apple)
ns.add_collection(pylon)
ns.add_collection(native)
ns.add_collection(clean)
ns.add_collection(wasm)
ns.add_collection(doc)
ns.add_collection(docker)

# After collections are set up, set the config.
ns.configure(config)
ns.configure(android.config)
ns.configure(apple.config)
ns.configure(pylon.config)
