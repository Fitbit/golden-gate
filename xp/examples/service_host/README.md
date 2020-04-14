# Service Host Example

This example consists of:
 * A command-line application that starts a stack with a UDP socket for transport, and runs a remote API shell with the blast service registered. That process uses a fifo as a remote shell transport.
 * A python program that bridges a pair of fifos with a websocket connection to a remote API relay.
 * A sample python application that can be used to send commands to the remote API
 * An HTML/JS page that can be used to test the functionality

## Setting up fifos

For each connection between a command-line process and the remote API relay, a pair of fifos is needed. Use the `mkfifo` command to create a fifo. This only needs to be done once, as fifos are persistent on the filesystem.

### Setup a fifo pair for the `hub` role

`mkfifo proc_to_relay_fifo_hub`

`mkfifo relay_to_proc_fifo_hub`

### Setup a fifo pair for the `node` role

`mkfifo proc_to_relay_fifo_node`

`mkfifo relay_to_proc_fifo_node`

## Running an example with a Hub and a Node

ensure that your PYTHONPATH has the required libraries:
example:
`export PYTHONPATH=~/Workspace/golden-gate/tests:~/Workspace/golden-gate/external/smo/python:~/Workspace/golden-gate/tests/remote_api`

NOTE: it is important to start the Hub role first. Also, if either the Hub or Node needs to be restarted, it is best to shut both down and restart cleanly.

### Start the remote API relay service
(from the Golden Gate root)

`node tools/websocket_proxy/relay.js`

### Start the fifo/websocket proxy for the `hub` role

`python xp/examples/service_host/service_host_proxy.py ws://localhost:8888/svchost/hub proc_to_relay_fifo_hub relay_to_proc_fifo_hub`

### Start the remote API shell for the `hub` role

`xp/build/cmake/native/examples/service_host/gg-service-host-example hub coap proc_to_relay_fifo_hub relay_to_proc_fifo_hub`

### Start the fifo/websocket proxy for the `node` role

`python xp/examples/service_host/service_host_proxy.py ws://localhost:8888/svchost/node proc_to_relay_fifo_node relay_to_proc_fifo_node`

### Start the remote API shell for the `node` role

`xp/build/cmake/native/examples/service_host/gg-service-host-example node coap proc_to_relay_fifo_node relay_to_proc_fifo_node`

## Executing commands from the command line

(ensure your PYTHONPATH is setup as explained above)

Using `remote_api_script_example.py`
That application takes as its arguments the Websocket URL to the remote API relay, followed by the RPC method name, followed by optional RPC parameters. Each parameter is specified as name=value, where value is either a string, or a number expressed as `#<n>`

examples:

`python xp/examples/service_host/remote_api_script_example.py ws://127.0.0.1:8888/svchost/node shell/get_handlers`

`python xp/examples/service_host/remote_api_script_example.py ws://127.0.0.1:8888/svchost/node blast/start packet_size=#200 packet_count=#100 packet_interval=#200`

## Using the HTML page

Loading `remote_api_shell_example.html` in Chrome (or other browser), you should see a web page with a button and URL input box. Enter the relay URL (ensure you pass a different URL to reflect whether you're connecting as a Hub or a Node).
Examples:

`ws://127.0.0.1:8888/svchost/node`

`ws://127.0.0.1:8888/svchost/hub`

Then click 'Connect'. The page will issue an RPC request to the remote API shell and retrieve the list of available RPC methods. For each method, a button and parameter text box will be created.
You can then use each one by clicking on the button with the method name. If an RPC method takes parameters, the parameters are passed with the same syntax as for the `remote_api_script_example.py` command-line tool described above.
Also, you can use `@{JSON}` to directly input JSON format.

examples:

`@{"key_info": {"key_identity": "myfriends", "key": "0102030405060708090AF1F2F3F4F5F6"}}`

