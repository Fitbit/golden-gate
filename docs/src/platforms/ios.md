iOS Platform
============

Setup
-----

Before you can open the XCode workspace, you will need to install(and build) the
project's dependencies. The [Carthage](https://github.com/Carthage/Carthage)
package and dependency manager is used for that. This bootstrap step is only
needed the first time you setup the project, or after 
Start by running
``` bash
$ inv apple.bootstrap
```

!!! info "Be patient..."
    When you run `inv apple.bootstrap` the first time, it may take a while, as
    it will likely pull in all the dependencies and build them from source.

After that bootstrap step has finished, the workspace can be opened.
``` bash
$ open platform/apple/GoldenGate.xcworkspace
```

Build The Host App
------------------

You can now build the `GoldenGateHost-iOS` app:

  * Pick `GoldenGateHost-iOS` and then select your connected device

You will need an Apple developer account in order to deploy the host app to a 
real phone.
Alternatively the simulator can be used, except if you need Bluetooth which 
Apple stopped supporting a while ago.

Run The Host App
----------------

In XCode, click Run (the "Play" icon).
See the [iOS Host App](../apps/ios/host_app.md) documentation for details.