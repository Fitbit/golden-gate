Multi-Stack example
-------------------

To run a test, start the following commands in separate shells:
* Shell 1 : `gg-multi-stack-example`
* Shell 2 : `gg-stack-tool --top 127.0.0.1 9000 9100 --bottom 127.0.0.1 6000 6100 --command-port 8000 node SNG`
* Shell 3 : `gg-stack-tool --top 127.0.0.1 9001 9101 --bottom 127.0.0.1 6001 6101 --command-port 8001 node SNG`
* Shell 4: `gg-stack-tool --top 127.0.0.1 9002 9102 --bottom 127.0.0.1 6002 6102 --command-port 8002 node SNG`
* Shell 5: `netcat -u -p 9000 127.0.0.1 9100`
* Shell 6: `netcat -u -p 7000 127.0.0.1 7100`
* Shell 7: `netcat -u -p 9001 127.0.0.1 9101`
* Shell 8: `netcat -u -p 7001 127.0.0.1 7101`
* Shell 9: `netcat -u -p 9002 127.0.0.1 9102`
* Shell 10: `netcat -u -p 7002 127.0.0.1 7102`

Shell 1 runs 3 stacks in one process, in Hub mode.
Shell 2, 3 and 4 each run a Node stack that communicate with the multi-stack process in shell 1.
Shells 5, 7 and 9 connect the terminal's I/O to the top of the three Node stacks.
Shells 6, 8 and 10 connect the terminal's I/O to the top of the three Hub stacks.
Shells 5 and 6 should be communicating with each other, as well as 7 and 8, 9 and 10.
