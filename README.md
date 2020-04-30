Project Golden Gate
====================

![Golden Gate bridge image - public domain photo via Good Free Photos](docs/src/golden_gate_bridge.jpg)

> :warning: **Work In Progress** 
>
> This documentation is not yet complete. As we migrate the project from
> an internal project to one that can be shared with others, we are busy
> migrating some of the documentation from internal Wiki pages, Google docs
> and slide decks.

Golden Gate is a portable cross-platform framework that offers reliable and
secure network communication between different combinations of embedded
devices, mobile applications and desktop applications over Bluetooth Low Energy.

The framework provides developers with a familiar model, allowing them to 
implement their functionality the same way they would in other networked
environments, leveraging familiar standards like CoAP, WebSockets, HTTP, MQTT,
TLS, and TCP/UDP/IP, even when the underlying operating system or transport 
doesn't natively support those.
It can run in many environments, from embedded systems with very limited
resources like wearables and IoT devices, to mobile and desktop. 
The initial focus is on Bluetooth Low Energy (BLE) connections, but the 
framework is general in nature, it is designed to work with any type of 
transport. BLE APIs on mobile operating systems like iOS and Android, as
well as most embedded APIs, only offer the limited functionality of BLE GATT;
so the Golden Gate framework's networking stack extends that low-level access, 
allowing a complete IP-based stack to be layered on top of it.

The project consists of:

 * A core library written in C
 * Language bindings for higher level languages, including Kotlin and Swift
 * A modular build system based on CMake
 * Code examples
 * Tools and applications
 * Support for test automation
 * Documentation

 By building the core libraries and sample applications included in the
 project, you'll be able to start experimenting right away with  different 
 combinations of devices connecting and communicating with each other with
 CoAP clients and servers, over a Bluetooth or WiFi connection, including:
  * iPhones and iPads
  * Android phones and tablets
  * Desktops and laptops (macOS, Linux, Windows)
  * Embedded development boards (Nordic nRF52840-DK, Espressif ESP32, ...)

By integrating the library in your own app or IoT device, leveraging some of 
the examples included in the project, you will be able to start building your 
own communicating applications and services.

Where To Start
--------------

Visit the online documentation (or build the doc from within the project).