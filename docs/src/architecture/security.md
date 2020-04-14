Security
========

Golden Gate supports secure end-to-end communications over its protocol stack.
In the standard configuration, a protocol stack includes as the next layer
after the CoAP application protocol a DTLS layer.

DTLS
----

DTLS is defined in [RFC6347](https://tools.ietf.org/html/rfc6347). It is
essentially a variant of TLS that is designed to work with UDP datagrams
instead of TCP streams.

### Cipher Suites

The library can be configured to support any cipher suite that is supported
by the DTLS engine that is selected at build time. In the current version,
the only DTLS engine support is MbedTLS, which supports an impressive range
of cipher suites.
In practices, when deployed on small devices, the DTLS engine is configured
with a small subset of all the supported cipher suite. The standard
configuration chosen is to enable the following cipher suites:

 * `TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256`
 * `TLS_PSK_WITH_AES_128_CCM`
 * `TLS_PSK_WITH_AES_128_GCM_SHA256`

The choice of PSK here is key to keeping the DTLS handshake simple and
lightweight for small devices, avoiding complex PKI certificate processing.

### Client and Server Roles

In the Golden Gate network topology, the stack running in the mobile
application is configured to be in the so-called Hub role, in which case its
DTLS protocol layer is a DTLS server. Conversely, the embedded device to
which the mobile application connects is configured to be in the so-called Node
role, in which case its DTLS protocol layer is a DTLS client. As soon as the
link layer connection is established, the DTLS handshake can start, with the
client sending its initial `hello`.

Key Management
--------------

With PSK authentication, the DTLS client and server must share a key at the
time a connection is established (the handshake).
The Golden Gate library does not impose a single mechanism for that key to
be shared. Any mechanism is valid, as long at the two peers agree on the key
identity and the key value.
Typical mechanisms for managing those authentication keys for embedded
devices is to receive them as part of an initial setup/pairing protocol, where
a manufacturing key may be used to secure a key delivery protocol, and/or
derived from that manufacturing key.
A typical mechanism for a mobile application to obtain the same authentication
key is to fetch it from an online service, over a secure HTTPS connection.

Bootstrap Key Mechanism
-----------------------

When deploying a system where the DTLS authentication key is provisioned in
the field, using a secure key distribution protocol, it is convenient to
make use of CoAP through the protocol stack in order to implement that key
distribution protocol. In order to keep the stack configuration the same
regardless of whether a key has yet been distributed or not, it is desirable
to still use DTLS during the key distribution protocol. Once the protocol
completes and the two peers share a key, the DTLS protocol is restarted and
a new, secure, connection is established using that key.
For that purpose, a fixed, non-secure, bootstrap key may be used.
This also allows easy testing of the stack for developers working on prototypes
or tests which may not have the ability to obtain "real" shared authentication
keys.
