Welcome to Golden Gate
======================

![Golden Gate bridge image - public domain photo via Good Free Photos](golden_gate_bridge.jpg)

!!! warning "Work In Progress"

    This documentation is not yet complete. As we migrate the project from
    an internal project to one that can be shared with others, we are busy
    migrating some of the documentation from internal Wiki pages, Google docs
    and slide decks.

Golden Gate is a portable cross-platform framework that offers reliable and
secure network communication between different combinations of embedded
devices, mobile applications and desktop applications.
It can run in many environments, from embedded systems with very limited
resources like wearables and IoT devices, to mobile and desktop applications.

The framework includes a core set of components that can be combined to build
IP-based network stacks. A stack running on a device or within a mobile app
connects to another, compatible stack running on a remote device or mobile app
over some transport or link, typically Bluetooth. On top of the stack are 
clients or servers that access or expose some functionality. In the current
version of the framework, the clients and servers are CoAP endpoints. In future
versions, other application protocols will be supported, like Web Sockets and
HTTP.

The project consists of:

 * A core library written in C
 * Language bindings for higher level languages, including Kotlin and Swift
 * A modular build system based on CMake
 * Code examples
 * Tools and applications
 * Support for test automation
 * Documentation

Documentation Sections
----------------------

 * [Introduction](introduction.md) - A high level introduction to the project
 * [Getting Started](getting_started.md) - How to get started building and using the framework
 * [Architecture](architecture/index.md) - Architecture of the framework
 * [Developer Guides](guides/index.md) - Collection of developer guides
 * [API Reference](api/index.md) - Detailed API documentation for C, Kotlin and Swift
 * [Platforms](platforms/index.md) - Platform-specific documentation
 * [Transports](transports/index.md) - Transport-specific documentation
 * [Release Notes](release_notes.md) - What's new and changed in each release
 * [Glossary](glossary.md) - Definition of terms
 * [FAQ](faq.md) - Frequently asked questions, and answers
