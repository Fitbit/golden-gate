Stack Tool
==========

The Stack Tool is a general purpose command-line tool that can be used to run a
Golden Gate stack in a process, and connect the stack's top and bottom ports to
UDP sockets so that the stack can communicate with other stacks and/or
applications, running on the same host or another host on the network.
On macOS, the bottom of the stack can alternatively be connected directly to
Bluetooth.  
The stacks may be full stacks (ex: DTLS <-> UDP <-> IP <-> Gattlink) or
subsets, even single element stacks. This makes it possible to split an entire
end-to-end communication chain into separate partial stacks that are assembled
into a complete stack by connecting the partial stacks over UDP.

Usage
-----

```
usage:
  gg-stack-tool [options] hub|node <stack-descriptor-or-command>

where <stack-descriptor-or-command> is either a stack descriptor or a command
string starting with a @ character

options:
  --top [coap] <send_host_ip> <send_port> <receive_port>
    Specify the IP address and port number to connect to the top of the stack (user).
    If the 'coap' option is used, packets sent and received through the top of the stack
    are assumed to be CoAP datagrams.
    (default: 127.0.0.1 9002 9003 as a hub and 127.0.0.1 9003 9002 as a node)

  --top blast <packet-count> <packet-size>
    If <packet-count> is not 0, attach a packet blaster to the top of the stack, with the
    given paket count and packet size, and start blasting, as well as printing stats for
    packets received from a remote blastera. If <packet-count> or <packet-size> is 0,
    don't start blasting, only print stats.

  --top tunnel
    Connect the top of the stack on an IP tunnel (typically only useful for stacks where the
    top of the stack produces/consumes IP packets).

  --bottom <send_host_ip> <send_port> <receive_port>
    Specify the IP address and port number to connect to the bottom of the stack (transport).
    (default: 127.0.0.1 9000 9001 as a hub and 127.0.0.1 9001 9000 as a node)

  --bottom bluetooth <bluetooth-device-id>|scan|node:<advertised-name>
    In the 'hub' role, connect the bottom of the stack directly to a Bluetooth peripheral,
    connecting to the device with ID <bluetooth-device-id> (obtained by scanning).
    Use 'scan' to only scan and display device IDs.
    In the 'node' role, accept connections from a Bluetooth central
    (this is mutually exclusive with the --bottom option above).

  --force-start
    Don't wait for a link up event from the transport before starting the stack (only
    valid when the bottom of the stack is bluetooth).

  --gattlink <max-fragment-size> <rx-window> <tx-window>
    Specify the Gattlink parameters.

  --dtls-key <key-identity>:<key>
    Where <key-identity> is an ASCII string, and <key> is 16 bytes in hex (32 characters)
    For the `hub` role, multiple --dtls-key options can be used to specify a list of
    keys. For the `node` role, only one key can be specified.

  --enable-header-compression
    Enable header compression

  --trace
    Show packets as they are received or sent from the top and bottom sockets.

  --command-port <command_port>
    Receive commands on port <command_port> (default: 7000 for hub, 7001 for node).

  --event-port <event_port>
    Send events on port <event_port> (default: 7100 for hub, 7101 for node).

commands:
    @reset                             : reset the stack
    @dtls-add-key:<key-identity>:<key> : add a DTLS key
    @bt-connect:<uuid>|scan   : connect to a bluetooth device or scan
    @bt-lc-set-mode:slow|fast : set the preferred link controller connection mode

NOTES:
  * Specify a port number of 0 for the send port of the top or bottom to indicate
    that the socket should send to the IP address and port number of the latest received
    packet instead of a fixed address and port.
  * Specify a port number as 0/X, with X non-zero, for the send port of the top, to
    indicate that CoAP requests going to through the top should be sent to port X, but CoAP
    responses should be sent to the port number from which they were received.
  * Specify a port number of 0 for the receive port of the top or bottom to indicate
    that the network stack should pick any available port number.
```

Bluetooth
---------

On macOS, the tool can interface directly with the operating system's Bluetooth stack.

### Bluetooth as a Hub

The `--bottom bluetooth <bluetooth-device-id>` option may be used to connect to a
bluetooth device running a compatible stack. `<bluetooth-device-id>` is a macOS-assigned
peripheral UUID. To obtain the bluetooth device ID for a specific device, first run the
tool with the option `--bottom bluetooth scan` and look for the device IDs printed on
the console. Once you have found the device you are looking for, re-run the tool with the
actual bluetooth device ID you want to connect to.

### Bluetooth as a Node

The `--bottom bluetooth node:<bluetooth-device-name>` option may be used to advertise
bluetooth services as a Node.

!!! tip
    In order to keep the console output reasonably quiet, it is recommended to limit the
    logging level to INFO, which can be done this way:
    ``` bash
    $ export "GG_LOG_CONFIG=plist:.level=INFO"
    ```

    or to trace the operations of the bluetooth transport:
    ``` bash
    $ export "GG_LOG_CONFIG=plist:gg.xp.app.stack-tool.core-bluetooth.level=ALL;.ConsoleHandler.filter=4;.level=INFO"
    ```

Examples
--------

In the following examples, we setup stacks or partial stacks connected together
so that UDP packets sent to port 5680 are sent through the stacks and come out
at the end as a UDP packet sent to localhost (127.0.0.1) on port 5683.
This way, we can use a regular CoAP server listening on port 5683, and a
regular CoAP client that sends request to the localhost (127.0.0.1) on port
5680, which will be forwarded through the stacks to the sever.

!!! note "Adding the tools to your `PATH`"
    In the following examples, we assume that the directories where the command
    line tools are build have been added your `PATH`. Alternatively, you can
    use the full path to the tool's executable instead of just the executable
    name.

### CoAP Client And Server Through A Stack

In these examples, we run two or more stack tool instances (in
separate shell processes), configured such that one of the stacks receives 
packets at its top, on port 5680, from a CoAP client, and communicates with 
another stack, itself possibly connected to another stack, and so on, until a 
last stack that sends packets to a CoAP server on port 5683.

Network topology:
```
 +--------+            +------------+   |            |            +--------+
 | CoAP   |---[5680]-->|            |-->| any number |---[5683]-->| CoAP   |
 | Client |            | Stack Tool |   | of Stack   |..          | Server |
 |        |<--[any ]---|            |<--| Tool       |<--[any ]---|        |
 +--------+            +------------+   | processes  |            +--------+
```

In each case, once the stacks are running, run the Python CoAP server from [CoAPthon](https://github.com/Tanganelli/CoAPthon)
``` bash
$ coapserver.py
```

And then run the CoAP client using the ``coap`` NodeJS tool from [coap-cli](https://github.com/mcollina/coap-cli)
``` bash
$ coap coap://127.0.0.1:5680/big
```

You should see the CoAP response printed on the console.
``` bash
$ coap coap://127.0.0.1:5680/big
(2.05)	Lorem ipsum dolor sit amet, ...
```

#### Configuration 1
Simple pair of complete stacks connect to each other on the bottom side.
   (run each of the following commands in a separate shell):
``` bash
$ gg-stack-tool --top coap 127.0.0.1 5683 0 hub DSNG
```
``` bash
$ gg-stack-tool --top coap 127.0.0.1 0 5680 node DSNG
```

#### Configuration 2
Completely deconstructed stacks (each stack runs a single layer of a complete
stack, so it is like having a stack where each layer runs in a separate process).

!!! note "Advanced example"
    This is an advanced example, intended to illustrate that you can combine
    multiple stacks together, but in practice you will typically use just one
    stack on each machine.

Run each of the following commands in a separate shell):
``` bash
$ gg-stack-tool --top coap 127.0.0.1 5683 0 --bottom 127.0.0.1 9009 9008 --trace hub D
```
``` bash
$ gg-stack-tool --top 127.0.0.1 9008 9009 --bottom 127.0.0.1 9007 9006 --trace --command-port 7001 hub SN
```
``` bash
$ gg-stack-tool --top 127.0.0.1 9006 9007 --bottom 127.0.0.1 9004 9005 --trace --command-port 7002 hub G
```
``` bash
$ gg-stack-tool --top 127.0.0.1 9002 9003 --bottom 127.0.0.1 9005 9004 --trace --command-port 7003 node G
```
``` bash
$ gg-stack-tool --top 127.0.0.1 9000 9001 --bottom 127.0.0.1 9003 9002 --trace --command-port 7004 node SN
```
``` bash
$ gg-stack-tool --top coap 127.0.0.1 0 5680 --bottom 127.0.0.1 9001 9000 --command-port 7005 --trace node D
```

Bluetooth Examples
------------------

These examples are only available on the mac, not Linux, because the stack tool
doesn't yet have native Bluetooth support on Linux desktops.
On the mac, the OS assigns a unique device ID to each Bluetooth device it can
connect to (instead of exposing the device's Bluetooth address). So the first
step is to use the stack tool to scan for devices, which will print on the 
console a list of compatible Bluetooth devices, along with their OS-assigned
device ID. Those device IDs can then be used to connect.

### Scanning

Run
``` bash
$ gg-stack-tool --bottom bluetooth connect hub DSNG
```

If there is a compatible device advertising, you should see on the console 
something like (see the tip above about setting your `GG_LOG_CONFIG` environment
variable to avoid having too many log lines clutter the console output):
```
Bluetooth Scan: gg-peripheral - ID = DB147848-D227-43A8-A7A7-3AF2530BABC5 RSSI = -34
```

In this example, we see a device advertising as `gg-peripheral` for which the
OS has assigned ID `DB147848-D227-43A8-A7A7-3AF2530BABC5`.

Bluetooth Hub Example
---------------------

In this example, we connect to a specific device over Bluetooth (device ID
DB147848-D227-43A8-A7A7-3AF2530BABC5 in this example) and we start a stack. 
The top of the stack is configured to proxy packets on port 5683,
so any CoAP client targeting the macOS host will end up being proxy'ed to the 
device.

```
 +--------+            +------------+   |           |      +-----------+
 | CoAP   |---[5683]-->|            |-->|           |----->| GG Device |
 | Client |            | Stack Tool |   | Bluetooth |      | with CoAP |
 |        |<--[any ]---|            |<--|           |<-----| server    |
 +--------+            +------------+   |           |      +-----------+
```

Run:
``` bash
$ gg-stack-tool --trace --top coap 127.0.0.1 0 5683 --bottom bluetooth DB147848-D227-43A8-A7A7-3AF2530BABC5 hub DSNG
```

If the device you are connecting to hosts a CoAP server with a `/helloworld` 
resource (as would be the case for the Pylon `gg-peripheral` app running on a
Nordic board), you can try a CoAP request like:
```
$ gg-coap-client get coap://127.0.0.1/helloworld
```

You should see:
```
=== Received response block, offset=0:
  code = 2.05
  token = 41A73AF1
  option 12 [Content-Format] (uint): 0
  payload size = 18
  payload:
  0000: Hello CoAP clien    48656C6C6F20436F415020636C69656E
  0016: t!                  7421                            
### Last block, we're done!
Hello CoAP client!
```

Bluetooth Node Example
----------------------

In this example, we start the stack and advertise bluetooth services under the name
'Jiji'. The stack is configured with DTLS, but no key other than the default 
bootstrap key. The top port is also configured to send CoAP requests out to a 
local CoAP server that is listening on port 6683.

```
 +--------+
 | CoAP   |
 | Server |<--+
 |        |   |
 +--------+   |
              |
 +--------+   +-[6683]--->+------------+   |           |      +-----------+
 | CoAP   |-----[5683]--->|            |-->|           |----->| GG Device |
 | Client |               | Stack Tool |   | Bluetooth |      | with CoAP |
 |        |<----[any ]----|            |<--|           |<-----| server    |
 +--------+               +------------+   |           |      +-----------+
```

Run:
``` bash
$ gg-stack-tool --trace --bottom bluetooth node:Jiji --top 127.0.0.1 0/6683 5683 node DSNG
```
