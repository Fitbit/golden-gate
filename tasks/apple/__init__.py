from invoke import Collection, task

from . import bindings
from . import carthage
from . import ios
from . import macos
from . import slather
from . import utils
from . import xcodebuild
from . import xp

@task(carthage.bootstrap)
def bootstrap(_ctx):
    """Get you ready to open the xcworkspace"""
    print("Your workspace is ready. Feel free to open it now:")
    print()
    print("$ open platform/apple/GoldenGate.xcworkspace")
    print()

@task
def clean(ctx):
    '''Blow away apple build specific directories'''
    ctx.run("rm -rf {}/apple/build".format(ctx.C.PLATFORM_DIR))
    ctx.run("rm -rf {}/xcode-*".format(ctx.C.BUILD_DIR))
    ctx.run("rm -rf {}".format(ctx.C.APPLE_BUILD_TEMP_DIR))
    ctx.run("rm -rf Carthage")
    ctx.run("rm -rf {}/generated/xcode-*/GoldenGateXP".format(ctx.C.BUILD_ROOT_DIR))


ns = Collection(bootstrap, clean)
ns.add_collection(bindings)
ns.add_collection(carthage)
ns.add_collection(ios)
ns.add_collection(macos)
ns.add_collection(slather)
ns.add_collection(utils)
ns.add_collection(xcodebuild)
ns.add_collection(xp)

config = {  # pylint: disable=C0103
    'carthage': {
        # Override this in `~/.invoke.yaml` if you don't need to debug dependencies.
        'dev': False,
        # Override this in `~/.invoke.yaml` to use Rome and speed up build time.
        'rome': False
    },
    'xcodebuild': {
        # Override this in `~/.invoke.yaml` if you use a different simulator normally.
        'destination': 'platform=iOS Simulator,name=iPhone 8,OS=latest',
        # Override this in `~/.invoke.yaml` if you want `xcpretty` on tests
        # but assertion failures and sigints will remain invisible.
        'xcpretty_for_tests': False,
        # Override this in `~/.invoke.yaml` if you want `xcpretty` on builds.
        'xcpretty_for_builds': False,
    }
}
