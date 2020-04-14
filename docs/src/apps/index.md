Sample Apps
===========

Mobile Host Apps
----------------

Mobile Host Apps are very simple sample apps that you can run on a mobile phone
or tablet. These apps allow you to scan for and connect to compatible devices,
phones or desktops, running a peer Golden Gate-based app. The Host Apps can be
configured to different stack configurations.  
Once connected, a few simple operations are possible, including sending test 
CoAP requests, and performing a 'blast' throughput test.

There is a [Host App for Android](android/host_app.md) and a 
[Host App for iOS](ios/host_app.md)

MyNewt Sample Apps
------------------

Two sample applications, which share a lot in common, are available for the 
reference MyNewt port (a.k.a "Pylon"):

  * `gg-peripheral` - a simple MyNewt application that configures the board to
     power-on bluetooth and advertise as a Peripheral 
     (a.k.a Node in the Golden Gate topology).
  * `gg-central` - a simple MyNewt application that configures the board to
     power-on bluetooth as a Central 
     (a.k.a Hub in the Golden Gate topology)

Both apps offer similar functionality, including a command-line interface to
configure logging, the stack, and start/stop operations.

See [MyNewt Apps](mynewt/mynewt_apps.md) for details.