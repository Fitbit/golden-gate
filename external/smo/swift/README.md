# SMO - Simple Multipurpose Objects (Swift)

See `README.md` in the root directory for more information.

## Prerequisites

The library itself is Swift 4.0 compatible.

The tests however rely on [SE-0185](https://github.com/apple/swift-evolution/blob/master/proposals/0185-synthesize-equatable-hashable.md) (part of *Swift 4.1*) to reduce boilerplate.

## Usage

### Using Codable

```
// Create a class/struct that conforms to Codable
struct Message: Codable {
    let sender: String
}

// decoding
let value = try FbSmoDecoder().decode(Message.self, from: data, using: .cbor)

// encoding
let encodedData = try FbSmoEncoder().encode(value, using: .cbor)
```

### Manually

```
// Create your complex FbSmo object
let smo: FbSmo = ["hello": "world"]

// encoding
let data = try smo.data(using: .cbor)

// decoding
let decodedValue = try FbSmo(data: data, using. cbor)
```

## Contributing

- Run `./configure` to generate that Xcode project.
- Run `swift build` to build the project (or build it from Xcode).
- Run `swift test` to run the tests (or run them in Xcode).
- Before committing code, please run `bin/format`.
