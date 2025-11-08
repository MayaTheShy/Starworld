#!/bin/bash
# Run stardust-overte-client with authentication

echo "Overte Domain Authentication"
echo "=============================="
echo ""
echo -n "Username: "
read username
echo -n "Password: "
read -s password
echo ""
echo ""

export OVERTE_USERNAME="$username"
export OVERTE_PASSWORD="$password"

echo "Connecting to Overte domain..."
./build/stardust-overte-client
