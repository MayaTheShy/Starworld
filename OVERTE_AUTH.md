# Overte Domain Authentication

## Current Status

✅ **Handshake Success!** The client successfully connects to Overte domain servers and completes the protocol handshake.

**Achievements:**
- Discovered correct protocol signature from mv.overte.org metaverse API
- Protocol version: `6xYA55jcXgPHValo3Ba3/A==` (eb1600e798dc5e03c755a968dc16b7fc)
- UDP communication established with domain server
- DomainConnectRequest packets properly formatted and sent
- **DomainList responses received** with assignment client endpoints
- Server accepts our protocol version and sends mixer information

**Technical Details:**
- Found 511 public Overte servers via https://mv.overte.org/server/api/v1/places
- Most servers use common protocol version `6xYA55jcXgPHValo3Ba3/A==`
- Successfully tested against local domain server (received EntityServer endpoints)
- Assignment client parsing implemented and working

**Next Steps:**
1. Parse assignment client list from DomainList packets
2. Connect to EntityServer UDP endpoint
3. Send EntityQuery packets to request world data
4. Parse EntityAdd/EntityEdit/EntityErase packets
5. Stream entities to Stardust XR

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
OVERTE_USERNAME="your_username" ./build/starworld
```

### Method 3: Export Variables
```bash
export OVERTE_USERNAME="your_username"
./build/starworld
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
STARWORLD_SIMULATE=1 ./build/starworld
```

## Protocol Implementation Status

✅ Domain UDP socket connection  
✅ NLPacket protocol format (sequence numbers, headers)  
✅ Protocol signature discovery from metaverse API  
✅ DomainConnectRequest packet structure  
✅ DomainList request/response parsing  
✅ **Handshake complete** - receiving DomainList with mixer endpoints  
✅ EntityServer endpoint discovery from DomainList  
⏳ EntityServer connection and EntityQuery packets  
⏳ Entity Add/Edit/Erase packet parsing  
⏳ Full property parsing (position, rotation, dimensions)  
⏳ Octree-based spatial streaming  
⏳ Avatar mixer integration  
⏳ Audio mixer integration  
❌ Signature-based authentication (optional for public servers)

## Recommendation

**Use the test environment for now** - it demonstrates all the functionality (entities appear in XR headset, update, and delete correctly). Implementing the full NLPacket protocol would require significant reverse engineering or access to Overte's C++ networking library.

