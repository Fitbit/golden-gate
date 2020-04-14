var isInteger =
    Number.isInteger ||
    function(value) {
        return (
            typeof value === "number" &&
            isFinite(value) &&
            Math.floor(value) === value
        );
    };

var arrayBufferIsView =
    ArrayBuffer.isView ||
    function(object) {
        return (
            object instanceof Uint8Array ||
            object instanceof Int8Array ||
            object instanceof Uint8ClampedArray ||
            object instanceof Uint16Array ||
            object instanceof Int16Array ||
            object instanceof Uint32Array ||
            object instanceof Int32Array ||
            object instanceof Float32Array ||
            object instanceof Float64Array ||
            object instanceof DataView
        );
    };

function Float(value) {
    this.value = value;
}

function bufferOfBytes() {
    var args = [];
    for (var _i = 0; _i < arguments.length; _i++) {
        args[_i] = arguments[_i];
    }
    var buffer = new ArrayBuffer(args.length);
    var bytes = new Uint8Array(buffer);
    for (var i = 0; i < args.length; i++) {
        bytes[i] = args[i];
    }
    return buffer;
}

function encodeHeader(majorType, length) {
    var h0 = majorType << 5;
    if (length < 24) {
        return bufferOfBytes(h0 | length);
    } else if (length < 0xff) {
        return bufferOfBytes(h0 | 24, length);
    } else if (length < 0xffff) {
        var h = new ArrayBuffer(3);
        var b = new DataView(h);
        b.setUint8(0, h0 | 25);
        b.setUint16(1, length);
        return h;
    } else if (length < 0xffffffff) {
        var h = new ArrayBuffer(5);
        var b = new DataView(h);
        b.setUint8(0, h0 | 26);
        b.setUint32(1, length);
        return h;
    } else if (length < 0xffffffffffffffff) {
        var h = new ArrayBuffer(9);
        var b = new DataView(h);
        b.setUint8(0, h0 | 27);
        var lo32 = length % 0x100000000;
        var hi32 = (length - lo32) / 0x100000000;
        b.setUint32(1, hi32);
        b.setUint32(5, lo32);
        return h;
    } else {
        throw new Error("Integer Overflow");
    }
}

function encodeArray(header, values) {
    var encodedItems = [];
    var bufferSize = header.byteLength;
    values.forEach(function(item) {
        var encodedItem = encode(item);
        bufferSize += encodedItem.byteLength;
        encodedItems.push(encodedItem);
    });
    var bytes = new Uint8Array(bufferSize);
    bytes.set(new Uint8Array(header), 0);
    var offset = header.byteLength;
    encodedItems.forEach(function(encodedItem) {
        bytes.set(new Uint8Array(encodedItem), offset);
        offset += encodedItem.byteLength;
    });
    return bytes.buffer;
}

function encodeFloat(value) {
    var bytes = new ArrayBuffer(9);
    var view = new DataView(bytes);
    view.setUint8(0, (7 << 5) | 27);
    view.setFloat64(1, value);
    return bytes;
}

function encodeInteger(value) {
    if (value >= 0) {
        return encodeHeader(0, value);
    } else {
        return encodeHeader(1, -(value + 1));
    }
}

function encode(value) {
    if (
        value instanceof Number ||
        value instanceof String ||
        value instanceof Boolean
    ) {
        value = value.valueOf();
    }
    if (value === false) {
        return bufferOfBytes((7 << 5) | 20);
    } else if (value === true) {
        return bufferOfBytes((7 << 5) | 21);
    } else if (value === null) {
        return bufferOfBytes((7 << 5) | 22);
    } else if (value === undefined) {
        return bufferOfBytes((7 << 5) | 23);
    }
    switch (typeof value) {
        case "number": {
            if (isInteger(value)) {
                if (value >= 0) {
                    return encodeHeader(0, value);
                } else {
                    return encodeHeader(1, -(value + 1));
                }
            } else {
                return encodeFloat(value);
            }
        }
        case "string": {
            var utf8 = new Uint8Array(3 * value.length);
            var offset = 0;
            for (var i = 0; i < value.length; i++) {
                var codePoint = value.charCodeAt(i);
                if (codePoint < 0x80) {
                    utf8[offset] = codePoint;
                    offset += 1;
                } else if (codePoint < 0x800) {
                    utf8[offset] = 0xc0 | (codePoint >> 6);
                    utf8[offset + 1] = 0x80 | (codePoint & 0x3f);
                    offset += 2;
                } else if (codePoint < 0xd800) {
                    utf8[offset] = 0xe0 | (codePoint >> 12);
                    utf8[offset + 1] = 0x80 | ((codePoint >> 6) & 0x3f);
                    utf8[offset + 2] = 0x80 | (codePoint & 0x3f);
                    offset += 3;
                } else {
                    codePoint =
                        (((codePoint & 0x3ff) << 10) |
                            (value.charCodeAt(++i) & 0x3ff)) +
                        0x10000;
                    utf8[offset] = 0xf0 | (codePoint >> 18);
                    utf8[offset + 1] = 0x80 | ((codePoint >> 12) & 0x3f);
                    utf8[offset + 2] = 0x80 | ((codePoint >> 6) & 0x3f);
                    utf8[offset + 3] = 0x80 | (codePoint & 0x3f);
                    offset += 4;
                }
            }
            var header = encodeHeader(3, offset);
            var bytes = new Uint8Array(header.byteLength + offset);
            bytes.set(new Uint8Array(header), 0);
            bytes.set(utf8.subarray(0, offset), header.byteLength);
            return bytes.buffer;
        }
        default: {
            if (Array.isArray(value)) {
                var header = encodeHeader(4, value.length);
                return encodeArray(header, value);
            } else if (
                value instanceof ArrayBuffer ||
                arrayBufferIsView(value)
            ) {
                if (arrayBufferIsView(value)) {
                    value = new Uint8Array(
                        value.buffer,
                        value.byteOffset,
                        value.byteLength
                    );
                } else {
                    value = new Uint8Array(value);
                }
                var header = encodeHeader(2, value.byteLength);
                var bytes = new Uint8Array(
                    header.byteLength + value.byteLength
                );
                bytes.set(new Uint8Array(header), 0);
                bytes.set(value, header.byteLength);
                return bytes.buffer;
            } else if (value instanceof Float) {
                return encodeFloat(value.value);
            } else {
                var keys = Object.keys(value);
                var header = encodeHeader(5, keys.length);
                var zipped_1 = [];
                keys.forEach(function(key) {
                    zipped_1.push(key);
                    zipped_1.push(value[key]);
                });
                return encodeArray(header, zipped_1);
            }
        }
    }
}

function decode(cbor) {
    var bytes;
    if (arrayBufferIsView(cbor)) {
        bytes = new DataView(cbor.buffer, cbor.byteOffset, cbor.byteLength);
    } else {
        bytes = new DataView(cbor);
    }
    var offset = 0;
    function decodeLength(additionalInformation) {
        var value;
        if (additionalInformation < 24) {
            value = additionalInformation;
        } else if (additionalInformation === 24) {
            value = bytes.getUint8(offset++);
        } else if (additionalInformation === 25) {
            value = bytes.getUint16(offset);
            offset += 2;
        } else if (additionalInformation === 26) {
            value = bytes.getUint32(offset);
            offset += 4;
        } else if (additionalInformation === 27) {
            value =
                bytes.getUint32(offset) * 0x100000000 +
                bytes.getUint32(offset + 4);
            offset += 8;
        } else {
            throw new Error("CBOR additional information value not supported");
        }
        return value;
    }
    function decodeUtf8(length) {
        var codePoints = [];
        while (length > 0) {
            var startOffset = offset;
            var codePoint = bytes.getUint8(offset++);
            if (codePoint & 0x80) {
                if (codePoint < 0xe0) {
                    codePoint =
                        ((codePoint & 0x1f) << 6) |
                        (bytes.getUint8(offset++) & 0x3f);
                } else if (codePoint < 0xf0) {
                    codePoint =
                        ((codePoint & 0x0f) << 12) |
                        ((bytes.getUint8(offset) & 0x3f) << 6) |
                        (bytes.getUint8(offset + 1) & 0x3f);
                    offset += 2;
                } else {
                    codePoint =
                        ((codePoint & 0x0f) << 18) |
                        ((bytes.getUint8(offset) & 0x3f) << 12) |
                        ((bytes.getUint8(offset + 1) & 0x3f) << 6) |
                        (bytes.getUint8(offset + 2) & 0x3f);
                    offset += 3;
                }
            }
            if (offset - startOffset > length) {
                throw new Error("Invalid CBOR string encoding");
            }
            length -= offset - startOffset;
            if (codePoint < 0x10000) {
                codePoints.push(codePoint);
            } else {
                codePoint -= 0x10000;
                codePoints.push(0xd800 | (codePoint >> 10));
                codePoints.push(0xdc00 | (codePoint & 0x3ff));
            }
        }
        if (length !== 0) {
            throw new Error("Invalid UTF-8 encoding");
        }
        return String.fromCharCode.apply(null, codePoints);
    }
    function decodeItem() {
        var firstByte = bytes.getUint8(offset++);
        var majorType = firstByte >> 5;
        var additionalInformation = firstByte & 0x1f;
        switch (majorType) {
            case 7: {
                switch (additionalInformation) {
                    case 21: {
                        return true;
                    }
                    case 20: {
                        return false;
                    }
                    case 22: {
                        return null;
                    }
                    case 23: {
                        return undefined;
                    }
                    case 26: {
                        var value = bytes.getFloat32(offset);
                        offset += 4;
                        return value;
                    }
                    case 27: {
                        var value = bytes.getFloat64(offset);
                        offset += 8;
                        return value;
                    }
                    default: {
                        throw new Error(
                            "CBOR additional information value " +
                                additionalInformation +
                                " not supported"
                        );
                    }
                }
            }
            case 0: {
                return decodeLength(additionalInformation);
            }
            case 1: {
                return -(decodeLength(additionalInformation) + 1);
            }
            case 3: {
                var length = decodeLength(additionalInformation);
                return decodeUtf8(length);
            }
            case 2: {
                var length = decodeLength(additionalInformation);
                var byteString = bytes.buffer.slice(offset, offset + length);
                offset += length;
                return new Uint8Array(byteString);
            }
            case 4: {
                var length = decodeLength(additionalInformation);
                var array = [];
                for (var i = 0; i < length; i++) {
                    array.push(decodeItem());
                }
                return array;
            }
            case 5: {
                var length = decodeLength(additionalInformation);
                var object = {};
                for (var i = 0; i < length; i++) {
                    var key = decodeItem();
                    if (typeof key !== "string") {
                        throw new Error("Only string keys are supported");
                    }
                    var value = decodeItem();
                    object[key] = value;
                }
                return object;
            }
            default: {
                throw new Error("CBOR major type not supported");
            }
        }
    }
    var decoded = decodeItem();
    if (offset !== cbor.byteLength) {
        throw new Error("Unexpected extra bytes");
    }
    return decoded;
}
