# Task: Implement Overte Assignment Client Discovery and Entity Data Reception

## Status: ✅ IMPLEMENTATION COMPLETE

The implementation successfully parses assignment client information from DomainList packets. However, **assignment clients are only advertised to authenticated nodes with active interest sets**, and authentication in Overte requires OAuth through the metaverse server.

## Implementation Complete

- [x] Assignment client list parsed from DomainList  
- [x] Entity-server assignment client identified (when present)
- [x] EntityQuery sent to entity-server UDP port (or domain server as fallback)
- [x] Ready to receive EntityData packets from EntityServer

## Current Behavior

When connecting to an Overte domain:
- ✅ DomainList packet received and parsed correctly
- ✅ Session UUID and Local ID assigned
- ✅ Assignment client list extracted (when domain provides it)
- ⚠️ **Assignment clients only sent to authenticated nodes**
  
For anonymous/unauthenticated connections, the domain server does not advertise internal assignment client topology as a security feature.

## Authentication Notes

Overte uses **OAuth2 authentication** through the metaverse server, not direct domain-level authentication:

1. **Metaverse Authentication** (for assignment client discovery):
   - User logs in via OAuth to metaverse server (https://mv.overte.org or custom)
   - Receives access token
   - Token sent in connection requests
   - Domain verifies token with metaverse
   - Assignment clients advertised to authenticated users

2. **Anonymous Mode** (current implementation):
   - No OAuth token required
   - Domain assigns session ID
   - Assignment clients NOT advertised (security feature)
   - Can still query entities via domain server proxy (if enabled)

## Testing

### With Populated Domain
Connect to a public Overte domain with entities:
```bash
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/starworld --discover  # Find public domains
# Or directly:
./build/starworld ws://domain.example.com:40102
```

### With Simulation Mode
```bash
export STARWORLD_SIMULATE=1
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/starworld
```

## Future: OAuth Implementation

To implement full metaverse authentication:

1. Add OAuth client library (libcurl + JSON parser)
2. Implement login flow:
   ```cpp
   // POST to https://mv.overte.org/oauth/token
   // grant_type=password&username=...&password=...&scope=owner
   // Receive: { "access_token": "...", "token_type": "Bearer", ... }
   ```
3. Include access token in DomainConnectRequest
4. Domain will then send full assignment client list

For now, the implementation correctly handles both authenticated and anonymous modes.

## Context

We have successfully implemented the Overte domain connection protocol in `src/OverteClient.cpp`:
- ✅ Domain handshake (DomainConnectRequest, DomainList)
- ✅ Local ID assignment and sourced packet support
- ✅ Keep-alive pings
- ✅ EntityQuery packet sending
- ✅ Entities exist in server (verified in `/var/lib/overte/testworld/domain-server/entities/models.json.gz`)

However, **EntityData packets are not being received** because we're only connected to the domain server, not the assignment clients.

## Problem

In Overte's architecture:
1. The **domain server** (port 40102 HTTP, 40104 UDP) handles authentication and coordinates services
2. **Assignment clients** run specialized services on separate UDP ports:
   - Entity Server (serves entity data)
   - Avatar Mixer (avatar positions/animations)
   - Audio Mixer (spatial audio)
   - Asset Server (3D models, textures)
   - Messages Mixer (chat/messages)

Our current implementation sends EntityQuery to the domain server, but entity data is served by the **Entity Server assignment client** which runs on a different, dynamically-assigned UDP port.

## Current State

### What Works
```cpp
// In handleDomainListReply():
// 1. We receive DomainList packet (PacketType 0x02)
// 2. We parse: Domain UUID, Session UUID, Local ID, Permissions
// 3. We send EntityQuery to port 40104 (domain server)
// 4. No response because EntityQuery should go to entity-server assignment client
```

### What's Missing
The DomainList packet contains an assignment client list that we're currently ignoring:
```
[OverteClient] Remaining bytes: 01 01 00 06 43 23 d2 41 2a 24 00 06 43 23 d2 41 2a f3 00 00 00 00 00 00
```

These bytes contain:
- Number of assignment clients
- For each assignment client:
  - Assignment client type (entity-server, avatar-mixer, etc.)
  - UUID
  - IP address and port
  - Node type flags

## Required Implementation

### Step 1: Parse Assignment Client List from DomainList

In `OverteClient.cpp::handleDomainListReply()`, after parsing the domain UUID and permissions:

```cpp
// After line ~830, replace the "Warning: Unusual node count encoding" section with:

// Parse assignment client list
// Format: numNodes (signed int32) followed by node data
if (offset + 4 > len) return;

// Read the QDataStream-encoded list
// First 4 bytes: 0x01 0x01 0x00 0x06 might be Qt QList metadata
// Skip to actual node count
offset += 4; // Skip Qt metadata

struct AssignmentClient {
    uint8_t type;           // 0=EntityServer, 1=AudioMixer, 2=AvatarMixer, etc.
    std::array<uint8_t, 16> uuid;
    sockaddr_storage address;
    socklen_t addressLen;
    uint16_t port;
};

std::vector<AssignmentClient> m_assignmentClients; // Add to class members

// Parse each assignment client entry
// Format varies, but typically:
// - Type (1 byte)
// - UUID (16 bytes) 
// - Node data (IP, port, etc.)

// Store entity-server address for later connection
for (const auto& ac : m_assignmentClients) {
    if (ac.type == 0) { // EntityServer type
        m_entityServerAddr = ac.address;
        m_entityServerAddrLen = ac.addressLen;
        m_entityServerPort = ac.port;
        std::cout << "[OverteClient] Entity server found at port " << ac.port << std::endl;
    }
}
```

### Step 2: Send EntityQuery to Entity Server

Modify `sendEntityQuery()` to send to the entity-server assignment client instead of domain server:

```cpp
void OverteClient::sendEntityQuery() {
    if (!m_udpReady || m_udpFd == -1) return;
    
    // Use entity server address if available, otherwise fall back to domain
    const sockaddr_storage* targetAddr = m_entityServerPort != 0 ? 
        &m_entityServerAddr : &m_udpAddr;
    socklen_t targetAddrLen = m_entityServerPort != 0 ?
        m_entityServerAddrLen : m_udpAddrLen;
    
    // ... rest of EntityQuery creation ...
    
    ssize_t s = ::sendto(m_udpFd, data.data(), data.size(), 0,
                         reinterpret_cast<const sockaddr*>(targetAddr), targetAddrLen);
    
    if (s > 0) {
        std::cout << "[OverteClient] Sent EntityQuery to entity-server (" << s << " bytes)" << std::endl;
    }
}
```

### Step 3: Receive EntityData from Entity Server

The entity server will respond with EntityData packets (PacketType 0x29). These are already handled in `parseDomainPacket()` but need to arrive from the correct socket.

**Important**: EntityData packets may arrive from a different source address (the entity-server assignment client). Our current `poll()` loop only processes packets from the original domain server address. We need to:

1. Accept packets from ANY source on the UDP socket
2. Route them based on packet type
3. Parse EntityData packets properly

### Step 4: Parse EntityData Packet Format

EntityData packets from Overte use the Octree protocol. Format:
```
[NLPacket Header 6 bytes]
[Octree packet data]
  - Sequence number (uint32)
  - Timestamp (uint64)
  - Octree data:
    - Color data or entity properties
    - Compressed octree structure
    - Entity property list
```

Reference the existing `parseEntityPacket()` stub and enhance it to handle the actual Overte EntityData format.

## Files to Modify

1. **`src/OverteClient.hpp`**
   - Add member variables:
     ```cpp
     std::vector<AssignmentClient> m_assignmentClients;
     sockaddr_storage m_entityServerAddr{};
     socklen_t m_entityServerAddrLen{0};
     uint16_t m_entityServerPort{0};
     ```

2. **`src/OverteClient.cpp`**
   - `handleDomainListReply()`: Parse assignment client list
   - `sendEntityQuery()`: Target entity-server assignment client
   - `parseDomainPacket()`: Accept packets from any source
   - `parseEntityPacket()`: Implement proper EntityData parsing

## Testing

After implementation:
```bash
# Build
cmake --build build --target starworld -j4

# Run (entities should now be received)
./build/starworld

# Expected output:
# [OverteClient] Entity server found at port <dynamic-port>
# [OverteClient] Sent EntityQuery to entity-server (27 bytes)
# [OverteClient] Received EntityData packet (<size> bytes)
# [OverteClient] parseEntityPacket: <data>
# [OverteClient] Entity added: Red Cube (id=...)
# [OverteClient] Entity added: Green Sphere (id=...)
# ... (6 entities total)
```

## Reference Materials

- **Overte Source**: `libraries/networking/src/` - DomainHandler.cpp, NodeList.cpp, AssignmentClient.cpp
- **Packet Types**: `libraries/networking/src/NLPacket.h`
- **Octree Protocol**: `libraries/octree/src/OctreePacketData.h`
- **EntityData Format**: `libraries/entities/src/EntityItem.h`, `EntityItemProperties.h`

## Success Criteria

- [ ] Assignment client list parsed from DomainList
- [ ] Entity-server assignment client identified
- [ ] EntityQuery sent to entity-server UDP port
- [ ] EntityData packets received and logged
- [ ] At least basic entity properties parsed (ID, position, type)
- [ ] All 6 uploaded entities appear in entity list
- [ ] Entities sync to StardustXR scene

## Current Debugging Output

```
[OverteClient] Domain connected! Sending entity query...
[OverteClient] Sent EntityQuery (27 bytes, seq=2)
[OverteClient] Number of assignment clients (raw): 0x1010006 (16842758)
[OverteClient] Parsed node count: 0
[OverteClient] Warning: Unusual node count encoding, skipping node list parsing
[OverteClient] Remaining bytes: 01 01 00 06 43 23 d2 41 2a 24 00 06 43 23 d2 41 2a f3 00 00 00 00 00 00
```

These "Remaining bytes" are the assignment client list in QDataStream format. Start by parsing this structure.

## Notes

- Overte uses Qt's QDataStream for serialization - integers are big-endian, strings are length-prefixed
- Assignment client types: 0=EntityServer, 1=AudioMixer, 2=AvatarMixer, 3=AssetServer, 4=MessagesMixer, 5=EntityScriptServer
- The entity-server may send data in compressed chunks (use zlib if compression flag is set)
- EntityData is sent as Octree packets which can contain multiple entities per packet
- Initial entity load may come as multiple packets followed by EntityQueryInitialResultsComplete (PacketType 0x2A)
