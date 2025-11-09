#!/bin/bash#!/bin/bash

# Backwards compatibility wrapper - scripts have moved to scripts/# Quick build and test script for Starworld

exec "$(dirname "$0")/scripts/build_and_test.sh" "$@"

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Starworld Build & Test Script ==="
echo

# 1. Build the Rust bridge
echo "[1/3] Building Rust bridge..."
cd "$SCRIPT_DIR/bridge"
cargo build --release
echo "✓ Rust bridge built successfully"
echo

# 2. Build the C++ client
echo "[2/3] Building C++ client..."
cd "$SCRIPT_DIR"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo "✓ C++ client built successfully"
echo

# 3. Verify the bridge library exists
echo "[3/3] Verifying build artifacts..."
BRIDGE_PATH="$SCRIPT_DIR/bridge/target/release/libstardust_bridge.so"
if [ -f "$BRIDGE_PATH" ]; then
    echo "✓ Bridge library found: $BRIDGE_PATH"
    ls -lh "$BRIDGE_PATH"
else
    echo "✗ Bridge library not found at: $BRIDGE_PATH"
    exit 1
fi

CLIENT_PATH="$SCRIPT_DIR/build/starworld"
if [ -f "$CLIENT_PATH" ]; then
    echo "✓ Client executable found: $CLIENT_PATH"
    ls -lh "$CLIENT_PATH"
else
    echo "✗ Client executable not found at: $CLIENT_PATH"
    exit 1
fi

echo
echo "=== Build Complete! ==="
echo
echo "To test with simulation mode:"
echo "  export STARWORLD_SIMULATE=1"
echo "  export STARWORLD_BRIDGE_PATH=$SCRIPT_DIR/bridge/target/release"
echo "  $SCRIPT_DIR/build/starworld"
echo
echo "To connect to an Overte server:"
echo "  export STARWORLD_BRIDGE_PATH=$SCRIPT_DIR/bridge/target/release"
echo "  $SCRIPT_DIR/build/starworld ws://domain.example.com:40102"
echo
