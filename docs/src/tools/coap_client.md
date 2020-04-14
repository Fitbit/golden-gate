CoAP Client Command Line Tool
=============================

The GG coap client tool is a simple command-line application that can be used
to send CoAP requests to a CoAP server and print the response that it receives.

Usage
-----

```
gg-coap-client get|put|post|delete [options] <uri>
  were URI must be of the form: coap://<ipv4-addr>[:port]/<path>[?<query>]

options:
  -q : be quiet (don't print out log/trace information)
  -p <payload-filename> : name of the file containing the payload to put/post
  -o <option>=<value> (supported options: 'Content-Format=<uint>',
   Block1=<uint>, Block2=<uint>, Size1=<uint>, Size2=<uint>, Start-Offset=<uint>,
   If-Match=<opaque-hex>)
  -b <preferred-block-size> (16, 32, 64, 128, 256, 512 or 1024 for block-wise)
     transfers, or 0 to force a non-blockwise transfer
  -s <filename> : save the response payload to <filename>
  -t <ack-timeout> : response ACK timeout, in milliseconds
  -r <max-resend-count>: maximum number of resends upon request timeouts
```


Examples
--------

### GET Request

Send a `GET` request to a server, running on the local host, for its 
`/helloworld` resource

``` bash
$ gg-coap-client get coap://127.0.0.1/helloworld
```

### POST Request

Send a `POST` request to a server, running on the local host, to its 
`/inbox` resource, using the contents of the file `payload.bin` as the request
payload.

``` bash
$ gg-coap-client post -p payload.bin coap://127.0.0.1/inbox
```
