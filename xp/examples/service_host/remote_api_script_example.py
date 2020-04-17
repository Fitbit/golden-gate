# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

from __future__ import print_function, absolute_import

import sys
from remote.websocket_transport import WebsocketTransport
from remote.remote import Remote
from remote.remote_request import RemoteRequest


def py23_str(value):
    try:  # Python 2
        return unicode(value)
    except NameError:  # Python 3
        return value

def send_request(remote, request):
    success, result = remote.send_request(request)

    if not success:
        print("Send Failed")
        return None

    return result


# Parse command-line parameters
# Invoke this example script with:
# <relay-url> <rpc-method> [<rpc-param-name>=<rpc-param-value>]*
# where <rpc-param-value> is either a string value, or a number value prefixed with '#'
# not supported in this example: passing a string value that starts with '#' ;-)
def main():
    relay_url = sys.argv[1]
    rpc_method = py23_str(sys.argv[2])
    rpc_params = dict([py23_str(p).split('=') for p in sys.argv[3:]])
    for param_name, param_value in rpc_params.items():
        if param_value.startswith('#'):
            rpc_params[param_name] = int(param_value[1:])

    # Create the transport layers
    websocket = WebsocketTransport(relay_url, 3600)

    # Create a remote object to communicate with
    remote = Remote(websocket)

    # Send the request
    print(">>> Request: ", rpc_method, rpc_params)
    remote_request = RemoteRequest(rpc_method, rpc_params)
    response = send_request(remote, remote_request)
    print("<<< Response:", response)


if __name__ == "__main__":
    main()
