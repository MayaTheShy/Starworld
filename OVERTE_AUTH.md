# Overte Domain Authentication

## Current Status

⚠️ **Protocol Compatibility Issue**: The Overte domain server uses the NLPacket protocol format which includes sequence numbers, packet headers, and specific framing that our current implementation doesn't support. The domain server is not responding to our connection requests.

### Working Alternative: Test Environment

The test environment using `tools/inject_test_entities.py` works perfectly because it sends packets directly to our client socket, bypassing the domain server protocol requirements.

**To use the test environment:**
```bash
# Terminal 1: Start the client
./build/stardust-overte-client

# Terminal 2: Inject test entities
python3 tools/inject_test_entities.py
```

This demonstrates the full entity lifecycle (create, update, delete) and entities are visible in the Stardust XR headset!

## Full Protocol Implementation (TODO)

To connect to a real Overte domain server, we need to implement:

###  1. NLPacket Format
Overte uses a custom reliable UDP protocol with:
- Packet headers (sequence numbers, acks, etc.)
- Message fragmentation/reassembly
- Reliable delivery guarantees

### 2. Proper Authentication Flow
1. Send DomainConnectRequest with NLPacket header
2. Receive DomainConnectionTokenRequest
3. Send authentication credentials
4. Receive DomainList with assignment client endpoints
5. Connect to each assignment client (EntityServer, Avatar, Audio)

### 3. Assignment Client Protocol
- Each mixer (EntityServer, AvatarMixer, AudioMixer) has its own handshake
- EntityServer requires octree-based spatial queries
- Proper node type identification in packets

## Running with Authentication (When Protocol is Implemented)

### Method 1: Interactive Script
```bash
./run_with_auth.sh
```
This will prompt you for username and password.

### Method 2: Environment Variables
```bash
OVERTE_USERNAME="your_username" ./build/stardust-overte-client
```

### Method 3: Export Variables
```bash
export OVERTE_USERNAME="your_username"
./build/stardust-overte-client
```

## Configuration

- **OVERTE_USERNAME**: Your Overte account username (optional; signature-based auth not yet implemented)
- **OVERTE_UDP_PORT**: Domain server UDP port (default: 40104)
- **STARWORLD_SIMULATE**: Set to "1" to enable simulation mode with demo entities

## Troubleshooting

### Protocol mismatch or denial

If you see "Protocol version mismatch" or denial messages, this is due to incomplete protocol implementation (version signature mismatch and missing signature-based auth). Use the test environment instead:

```bash
# Works perfectly - bypasses domain server
python3 tools/inject_test_entities.py
```

### Check Domain Server Status

```bash
# Check if domain server is running
ps aux | grep domain-server

# Check UDP port
sudo ss -ulnp | grep domain-server
```

### Test with Simulation Mode

```bash
STARWORLD_SIMULATE=1 ./build/stardust-overte-client
```

## Protocol Implementation Status

✅ Domain UDP socket connection  
✅ Authentication packet structure  
✅ DomainList request/response parsing  
✅ EntityServer discovery logic  
✅ EntityQuery packets  
✅ Entity Add/Edit/Erase parsing  
✅ **Working test environment** (Python injection)  
❌ NLPacket protocol headers  
❌ Reliable UDP (sequence numbers, acks)  
❌ Domain server handshake (not receiving responses)  
⏳ Full property parsing (position, rotation, dimensions)  
⏳ Octree-based spatial streaming  
⏳ Avatar mixer integration  
⏳ Audio mixer integration

## Recommendation

**Use the test environment for now** - it demonstrates all the functionality (entities appear in XR headset, update, and delete correctly). Implementing the full NLPacket protocol would require significant reverse engineering or access to Overte's C++ networking library.

