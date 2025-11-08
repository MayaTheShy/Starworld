#!/bin/bash
# Run stardust-overte-client with authentication

echo "Overte Domain Authentication"
echo "=============================="
echo ""
echo -n "Username: "
read username
echo "(Password no longer sent; Overte uses keypair signatures. Username is optional for now.)"
echo ""
export OVERTE_USERNAME="$username"

echo "Connecting to Overte domain..."
./build/stardust-overte-client
