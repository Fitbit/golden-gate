#####################################################################
# Simple fifo <-> WebSocket proxy
#
# Invoke with: <ws_url> <proc-to-relay-fifo> <relay-to-proc-fifo>
#####################################################################
from __future__ import print_function
import sys
import base64
import websocket

websocket.enableTrace(True)

def main():
    print("<<< opening process -> relay fifo")
    proc_to_relay_fifo = open(sys.argv[2], "r", 0)
    print(">>> opening process <- relay fifo")
    relay_to_proc_fifo = open(sys.argv[3], "w", 0)

    print("=== opening websocket")
    ws = websocket.WebSocket()
    ws.connect(sys.argv[1])
    print("=== connected")

    while True:
        message = ws.recv()

        encoded = base64.b64encode(message)
        print("<<<", encoded, len(encoded))
        relay_to_proc_fifo.write(encoded + "\n")

        response = proc_to_relay_fifo.readline()
        print(">>>", response, len(response))
        decoded = base64.b64decode(response)
        ws.send_binary(decoded)


if __name__ == "__main__":
    main()
