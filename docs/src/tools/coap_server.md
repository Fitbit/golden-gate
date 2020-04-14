CoAP Server Command Line Tool
=============================

The `gg-coap-server` tool is a very simple CoAP server that can be used as 
a test endpoint in examples, and as an easy to understand illustration of how
to use some of the C API for CoAP.

Usage:
```
gg-coap-server [--port <port>]
```

The `--port` option may be used to specify that the server should listen on a
UDP port other than the default (which is port 5683).

The server hosts a simple text resource at path `/hello` (with an alias at
`/helloworld`). A `GET` request to that resource returns the text
"Hello, World".  
Furthermore, if a client requests a subpath of `/hello`, such
as `/hello/name`, the server will respond with the text "Hello, name".

The server also hosts a CoAP Test Service. Documentation TBD.