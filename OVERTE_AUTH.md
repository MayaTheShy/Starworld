# Overte Domain Authentication

## Running with Authentication

The Stardust-Overte client now supports domain authentication. Use one of the following methods:

### Method 1: Interactive Script
```bash
./run_with_auth.sh
```
This will prompt you for username and password.

### Method 2: Environment Variables
```bash
OVERTE_USERNAME="your_username" OVERTE_PASSWORD="your_password" ./build/stardust-overte-client
```

### Method 3: Export Variables
```bash
export OVERTE_USERNAME="your_username"
export OVERTE_PASSWORD="your_password"
./build/stardust-overte-client
```

## Configuration

- **OVERTE_USERNAME**: Your Overte domain username
- **OVERTE_PASSWORD**: Your Overte domain password or access token
- **OVERTE_UDP_PORT**: Domain server UDP port (default: 40104)
- **STARWORLD_SIMULATE**: Set to "1" to enable simulation mode with demo entities

## Troubleshooting

If you're not receiving entities:

1. **Check authentication**: Make sure you've set up a user account in the Overte domain web interface (http://localhost:40102/settings/)

2. **Check domain server is running**:
   ```bash
   ps aux | grep domain-server
   sudo ss -ulnp | grep domain-server
   ```

3. **Test with simulation mode first**:
   ```bash
   STARWORLD_SIMULATE=1 ./build/stardust-overte-client
   ```

4. **Check for connection denied messages** in the output

## Protocol Implementation Status

✅ Domain connection handshake
✅ Authentication (username/password)
✅ DomainList request/response
✅ EntityServer discovery
✅ EntityQuery packets
✅ Entity Add/Edit/Erase parsing
⏳ Full property parsing (position, rotation, dimensions)
⏳ Octree-based spatial streaming
⏳ Avatar mixer integration
⏳ Audio mixer integration
