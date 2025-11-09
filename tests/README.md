This folder contains a minimal C++ test harness for starworld.

Build target: stardust-tests

What it checks:
- Protocol signature stability: compares NLPacket::computeProtocolVersionSignature() against the expected value for the vendored Overte commit.
- Domain discovery parsing: validates that JSON containing Vircadia- or Overte-style fields is parsed into host/port pairs correctly.

Run:
  ./build/stardust-tests

Exit code 0 indicates PASS.
