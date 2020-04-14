const gattlinkServiceUUID = BluetoothUUID.getService(
    "abbaff00-e56a-484c-b832-8b17cf6cbfe8"
);
const gattlinkRxCharacteristicUUID = BluetoothUUID.getCharacteristic(
    "abbaff01-e56a-484c-b832-8b17cf6cbfe8"
);
const gattlinkTxCharacteristicUUID = BluetoothUUID.getCharacteristic(
    "abbaff02-e56a-484c-b832-8b17cf6cbfe8"
);

const linkStatusServiceUUID = BluetoothUUID.getService(
    "abbafd00-e56a-484c-b832-8b17cf6cbfe8"
);
const linkStatusConnectionConfigurationCharacteristicUUID = BluetoothUUID.getService(
    "abbafd01-e56a-484c-b832-8b17cf6cbfe8"
);
const linkStatusConnectionStatusCharacteristicUUID = BluetoothUUID.getService(
    "abbafd02-e56a-484c-b832-8b17cf6cbfe8"
);
const linkStatusSecureCharacteristicUUID = BluetoothUUID.getService(
    "abbafd03-e56a-484c-b832-8b17cf6cbfe8"
);

const gattConfirmationServiceUUID = BluetoothUUID.getService(
    "ac2f0045-8182-4be5-91e0-2992e6b40ebb"
);
const gattConfirmationEphemeralCharacteristicPointerCharacteristicUUID = BluetoothUUID.getCharacteristic(
    "ac2f1045-8182-4be5-91e0-2992e6b40ebb"
);

let gattlinkRxCharacteristic;
const gattlinkRxQueue = [];
let goldenGateWorkTimer;

async function scanBluetooth() {
    try {
        console.log("=== Requesting device");
        const filters = [{ services: [gattlinkServiceUUID] }];

        const device = await navigator.bluetooth.requestDevice({
            filters,
            optionalServices: [
                linkStatusServiceUUID,
                gattConfirmationServiceUUID
            ]
        });

        console.log("device selected:", device);

        // attach a disconnection handler
        device.addEventListener("gattserverdisconnected", onDisconnected);

        console.log("connecting...");
        const server = await device.gatt.connect();

        // obtain the Link Status service in the background
        server
            .getPrimaryService(linkStatusServiceUUID)
            .then(onLinkStatusServiceFound);

        console.log("getting Gattlink Service...");
        const gattlinkService = await server.getPrimaryService(
            gattlinkServiceUUID
        );

        console.log("getting Gattlink RX characteristic...");
        gattlinkRxCharacteristic = await gattlinkService.getCharacteristic(
            gattlinkRxCharacteristicUUID
        );
        console.log(`--- characteristic:`, gattlinkRxCharacteristic.properties);

        console.log("getting Gattlink TX characteristic...");
        const gattlinkTxCharacteristic = await gattlinkService.getCharacteristic(
            gattlinkTxCharacteristicUUID
        );
        console.log(`--- characteristic:`, gattlinkTxCharacteristic.properties);

        console.log("subscribing to Gattlink TX...");
        await gattlinkTxCharacteristic.startNotifications();
        gattlinkTxCharacteristic.addEventListener(
            "characteristicvaluechanged",
            onGattlinkTx
        );
        console.log("subscribed");

        // now we can start the stack
        ccall("web_bluetooth_start_stack");
        performAllNonblockingWork();
    } catch (error) {
        console.error("something went wrong", error);
    }
}

function onDisconnected(event) {
    console.warn("!!! Device Disconnected");
    gattlinkRxCharacteristic = undefined;
    if (goldenGateWorkTimer) {
        clearTimeout(goldenGateWorkTimer);
        goldenGateWorkTimer = undefined;
    }
    gattlinkRxQueue.splice(0);
}

function onGattlinkTx(event) {
    const data = event.target.value;
    console.log("+++ Gattlink TX data", data);

    // copy data to the heap
    const numBytes = data.byteLength;
    const ptr = Module._malloc(numBytes);
    const heapBytes = new Uint8Array(Module.HEAPU8.buffer, ptr, numBytes);
    heapBytes.set(new Uint8Array(data.buffer));

    // send the data to the transport
    ccall(
        "web_bluetooth_send_to_transport",
        "number",
        ["number", "number"],
        [heapBytes.byteOffset, data.byteLength]
    );
    performAllNonblockingWork();

    // done with this heap buffer
    Module._free(ptr);
}

function onGattlinkRx(dataOffset, dataSize) {
    if (!gattlinkRxCharacteristic) {
        console.warn("!!! no Gattlink RX, dropping");
        return;
    }

    // copy the data from the heap into a new typed array packet
    const packet = new Uint8Array(dataSize);
    packet.set(HEAPU8.subarray(dataOffset, dataOffset + dataSize));

    // queue the packet
    console.log(`>>> Queuing ${dataSize} bytes packet to Gattlink RX`);
    const queueWasEmpty = gattlinkRxQueue.length === 0;
    gattlinkRxQueue.push(packet);

    // prime the queue pump if the queue was empty
    if (queueWasEmpty) {
        pumpGattlinkRxQueue();
    }
}

function pumpGattlinkRxQueue() {
    if (gattlinkRxQueue.length === 0) {
        // nothing to do
        console.log("--- Gattlink RX queue empty");
        return;
    }
    const packet = gattlinkRxQueue[0];
    console.log("*** Gattlink RX packet sent");
    gattlinkRxCharacteristic.writeValue(packet).then(_ => {
        // remove the packet from the queue
        gattlinkRxQueue.shift();

        // try to pump some more
        pumpGattlinkRxQueue();
    });
}

function onStackDataReceived(dataOffset, dataSize) {
    const packet = new DataView(
        HEAPU8.buffer,
        HEAPU8.byteOffset + dataOffset,
        dataSize
    );
    console.log(
        `=== Received ${dataSize} bytes from the top of the stack`,
        packet
    );
}

function onLinkStatusConnectionConfigurationChanged(event) {
    const data = event.target.value;
    console.log("*** Link Status Connection Configuration", data);
}

function onLinkStatusConnectionStatusChanged(event) {
    const data = event.target.value;
    console.log("*** Link Status Status Configuration", event);
}

async function onLinkStatusServiceFound(service) {
    console.log("found Link Status service");

    // subscribe to the Connection Configuration characteristic
    console.log("subscribing to the Connection Configuration characteristic");
    const connectionConfigCharacteristic = await service.getCharacteristic(
        linkStatusConnectionConfigurationCharacteristicUUID
    );
    await connectionConfigCharacteristic.startNotifications();
    connectionConfigCharacteristic.addEventListener(
        "characteristicvaluechanged",
        onLinkStatusConnectionConfigurationChanged
    );
    connectionConfigCharacteristic.readValue(
        onLinkStatusConnectionConfigurationChanged
    );

    // subscribe to the Connection Status characteristic
    console.log("subscribing to the Connection Status characteristic");
    const connectionStatusCharacteristic = await service.getCharacteristic(
        linkStatusConnectionStatusCharacteristicUUID
    );
    await connectionStatusCharacteristic.startNotifications();
    connectionStatusCharacteristic.addEventListener(
        "characteristicvaluechanged",
        onLinkStatusConnectionStatusChanged
    );
    connectionStatusCharacteristic.readValue(
        onLinkStatusConnectionStatusChanged
    );

    // try to read the Secure characteristic
    // console.log("trying to read Secure characteristic");
    // const secureCharacteristic = await service.getCharacteristic(linkStatusSecureCharacteristicUUID);
    // try {
    //     await secureCharacteristic.readValue();
    // } catch(error) {
    //     console.warn("failed to read secure characteristic:", error);
    // }
}

function initBindings() {
    console.log("=== START ===");
    ccall(
        "web_bluetooth_initialize",
        "number",
        ["string"],
        ["plist:gg.xp.gattlink.protocol.level=ALL;.level=INFO"]
    );
    performAllNonblockingWork();
}

function performAllNonblockingWork() {
    while (true) {
        const maxWait = ccall("web_bluetooth_do_work");
        if (maxWait > 0) {
            console.log("max wait", maxWait);
            if (goldenGateWorkTimer) {
                clearTimeout(goldenGateWorkTimer);
            }
            goldenGateWorkTimer = setTimeout(
                performAllNonblockingWork,
                maxWait
            );
            return;
        }
    }
}
