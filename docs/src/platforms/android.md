Android Platform
================

Setup
-----

### Java On Linux
If you're on a Linux host, make sure you have jre 8. 
For example, on ubuntu:
``` bash
$ sudo apt install openjdk-8-jdk
```

!!! tip Multiple JDKs
    If you have multiple jdk's installed, select jre 8 with:
    ```
    # print available versions
    $ update-java-alternatives --list

    # select openjdk-8, eg:
    $ sudo update-java-alternatives --set /usr/lib/jvm/java-1.8.0-openjdk-amd64
    ```

### Install Android Studio

Download Android Studio or at least the Android command line tools from
[Google](https://developer.android.com/studio/index.html)

#### On Linux

After installing (to eg `~/android-studio`), launch studio and follow the
wizard. It will prompt for a path to install the SDK component to; this can
be anywhere, eg `~/android-sdk`.

### Install The Android SDK Components

From Android studio, open the SDK manager (the icon looks
like a down arrow and the head of the Android logo) and install Android 8.1,
Android 8.0, Android 5.0, Android 7.1, NDK, Android SDK Tools, Android SDK
Platform-Tools, Android Support Library, LLDB, CMake, Android SDK
Build-Tools, and everything under the Support Library header. You can skip
the emulator images to save some disk space.

Check the project's gradle files to see which version of the NDK is referenced
(the current version should be: 21.0.6113669)

Alternatively, on Linux, you can install the SDK components from the command line:

```bash
$ sdk/tools/bin/sdkmanager \
    'platforms;android-26' \
    'platforms;android-21' \
    'platforms;android-25' \
    'platforms;android-26' \
    'platforms;android-27' \
    'platforms;android-28' \
    'platforms;android-29' \
    'ndk;21.0.6113669' \
    'cmake;3.6.4111459' \
    'lldb;3.0' \
    'build-tools;27.0.3' \
    'platform-tools' \
    'extras;m2repository;com;android;support;constraint;constraint-layout;1.0.2' \
    'extras;m2repository;com;android;support;constraint;constraint-layout-solver;1.0.2' \
    'extras;android;m2repository' \
    'extras;google;m2repository'
```

### Setup You Environment

Set the `ANDROID_HOME` environment variable to point to the SDK directory (eg
`export ANDROID_HOME=~/android-sdk` in your `~/.bashrc`)

Build The Core Library And Host App
-----------------------------------

To build the core library, run
``` bash
$ inv android.library.build
```

To build the Host App, open Android Studio and import the Golden Gate project,
which lives in `platform/android/goldengate`

Now you can run and/or debug the app from Android studio.

Alternatively, you can build the Host App from the command line:
``` bash
$ inv android.host.build
```
which produces an APK at
   `platform/android/goldengate/app/build/outputs/apk/debug/app-debug.apk` 
which you can install with `adb install app-debug.apk`

If you wish to build the library and Host App with one command, run:
``` bash
$ inv android.build
```

Run The Host App
----------------

See the [Android Host App](../apps/android/host_app.md) documentation.