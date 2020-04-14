MyNewt Port
===========

Two apps are included, one where the board runs as a Bluetooth central, and one
where it runs as a peripheral. They are both built the same way.
To build, flash and run the apps, you need to have the `newt` tool as 
well as the Segger JLink tools installed. 
On a mac, the `newt` tool should have been installed for you when you ran the 
`brew bundle` setup command. 
If you are using the docker image, it is also installed. 
If you want to install it separately, visit the [MyNewt installation](https://mynewt.apache.org/latest/get_started/native_install/)
instructions. You will need version 1.7 or later of the `newt` tool.

The instructions here assume that you are using the [Nordic nRF52840 DK Development Kit](https://www.nordicsemi.com/Software-and-Tools/Development-Kits/nRF52840-DK). You should be able to run on 
different boards (with Bluetooth) with minor adjustments.

Prepare You Board
-----------------

Before you can flash and run the `gg-central` or `gg-peripheral` app on your
board, you will need to perform a one-time setup of your board, where the flash
will be initialized and the bootloader installed.
Ensure your board is plugged in to a USB port on your system and turned on.
Run
``` bash
$ pylon.provision
```
This will setup the flash as well as build and flash the bootloader.

!!! bug "Newt 1.7 bug"
    A bug in version 1.7 of `newt` exists, where it will print an error message
    after installing the bootloader dependency. You can safely ignore that
    error message.
    
Build The Apps
--------------

Build the `gg-peripheral` app:
``` bash
$ pylon.apps.gg-peripheral.build
```

Build the `gg-central` app:
``` bash
$ pylon.apps.gg-central.build
```

!!! tip "MyNewt dependencies"
    If you have not built any of the Pylon projects earlier, the MyNewt 
    dependencies should be installed for you automatically. If that is not the
    case, you can install them manually with `pylon.apps.gg-central.install-project`
    and `pylon.apps.gg-peripheral.install-project`.

Run The Apps
------------

See the [MyNewt Apps](../apps/mynewt/mynewt_apps.md) documentation for details.