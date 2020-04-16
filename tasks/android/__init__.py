"""Android related tasks"""
import os

from invoke import Collection, task

from . import library
from . import host

@task(library.build, host.build)
def build(_ctx):
    '''Build All Android tasks'''

@task
def test(ctx, coverage=True, sonarqube=False):
    '''Runs tests'''
    bindings_dir = os.path.join(ctx.C.PLATFORM_DIR, "android", "goldengate", "GoldenGateBindings")
    coverage_dir = os.path.join(bindings_dir, '.externalNativeBuild', 'cmake', 'debug',
                                'unix', 'CMakeFiles', 'xp.dir')

    if coverage:
        ctx.run("rm -rf coverage/android")
        ctx.run("mkdir -p coverage/android")
        # Generate initial coverage data to capture untested files
        ctx.run("lcov --base-directory {dir} --directory {dir} --zerocounters -q".format(dir=coverage_dir))
        with ctx.cd(bindings_dir):
            ctx.run("../gradlew assembleDebugUnitTest")
        ctx.run("lcov --initial --base-directory {dir} --directory {dir} -c ".format(dir=coverage_dir) +
                "-o coverage/android/initial.info")

    command = "Coverage" if coverage else "UnitTest"
    bindings_command = "GoldenGateBindings:testDebug" + command
    manager_command = "GoldenGateConnectionManager:testDebug" + command
    rx_bitgatt_command = "RxFitbitGatt:testDebug" + command
    link_controller_command = ":linkController:testDebug" + command
    host_command = "app:testDebugUnitTest"
    remoteapi_command = "remoteapi:testDebug" + command
    root_dir = os.path.join(ctx.C.PLATFORM_DIR, "android", "goldengate")
    tasks = []
    tasks.append(bindings_command)
    tasks.append(manager_command)
    tasks.append(rx_bitgatt_command)
    tasks.append(host_command)
    tasks.append(remoteapi_command)
    tasks.append(link_controller_command)
    if sonarqube:
        tasks.append("sonarqube -Dsonar.host.url=https://sonarqube.site-ops.fitbit.com")

    with ctx.cd(root_dir):
        ctx.run("./gradlew " + " ".join(tasks))

    # analyze coverage after running tests
    if coverage:
        # Create code coverage analysis, merging initial coverage data.
        # Exclude external libraries, examples and apps.
        # Generate html report and cobertura compatible xml for CI.
        ctx.run('lcov --base-directory {dir} --directory {dir}'
                ' -c -o coverage/android/test.info'.format(dir=coverage_dir))

        with ctx.cd("coverage/android"):
            ctx.run("lcov -a initial.info -a test.info -o intermediate.info")
            ctx.run("lcov -e intermediate.info \"*/src/main/*\" -o final.info")

        ctx.run("genhtml -o coverage/android/html -t \"Golden Gate test coverage\" " +
                "--num-spaces 4 coverage/android/final.info")
        ctx.run('scripts/lcov_cobertura/lcov_cobertura.py coverage/android/final.info --base-dir {dir} '
                .format(dir=coverage_dir) + '--output coverage/android/cobertura.xml')

@task
def documentation(ctx):
    '''Generate KDoc documentation for bindings and connection manager'''
    if not ctx.android.sdk:
        return
    with ctx.cd(os.path.join(ctx.C.ROOT_DIR, "platform", "android", "goldengate")):
        ctx.run("./gradlew dokka")

@task
def clean(ctx):
    '''Blow away android build specific directories'''
    if not ctx.android.sdk:
        return

    # clean platform artifacts
    with ctx.cd(os.path.join(ctx.C.ROOT_DIR, "platform", "android", "goldengate")):
        ctx.run("./gradlew clean")

    # clean cmake artifacts
    ctx.run("rm -rf {}/android".format(ctx.C.BUILD_DIR))

ns = Collection(build, test, clean, documentation)  # pylint: disable=C0103
ns.add_collection(host)
ns.add_collection(library)

config = {  # pylint: disable=C0103
    "android": {
        "sdk": os.environ.get("ANDROID_HOME", None)
    }
}
