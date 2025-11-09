# Starworld Test Suite

This folder contains the C++ test harness for Starworld.

## Build Target

`starworld-tests` - Built alongside the main executable

## What It Tests

1. **Protocol signature stability**: Compares `NLPacket::computeProtocolVersionSignature()` against the expected value for the vendored Overte protocol
2. **Domain discovery parsing**: Validates JSON parsing from Vircadia/Overte metaverse directories into host/port pairs

## Running Tests

```bash
./build/starworld-tests
```

Exit code 0 indicates PASS.

## Test Files

- `TestHarness.cpp` - Main test implementation with protocol and discovery tests
