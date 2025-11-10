# Overte Network Protocol Investigation

## Current Status (Nov 10, 2025)

**⚠️ HMAC VERIFICATION DEADLOCK - SERVER CONFIGURATION ISSUE**

### Working ✅
- **DomainConnectRequest packet format** - Matches official Overte client implementation
- **Connection established** - Server responds with DomainList packet
- **Local ID assignment** - Server assigns a valid Local ID correctly
- **Local ID parsing** - Fixed to read little-endian uint16 at correct offset (bytes 34-35)
- **Symmetric socket creation** - Server creates correct socket to our actual address
- **Packet version negotiation** - Using correct version 27 (DomainConnectRequest_SocketTypes)
- **Protocol signature** - MD5 hash `eb1600e798dc5e03c755a968dc16b7fc` matches server
- **Packet structure** - All fields properly ordered and serialized per Qt QDataStream format
- **Source ID handling** - Little-endian uint16 at offset 6-7 in sourced packet header
- **HMAC-MD5 implementation** - Correctly calculates hash using OpenSSL with null UUID as key
- **Packet hash insertion** - Properly reserves 16-byte slot and moves payload

### Not Working / Blocked ❌
- **Connection persistence** - Still killed after 11-18 seconds as "silent node"
- **HMAC verification** - Server rejects all sourced packets due to hash mismatch
- **Keep-alive mechanism** - Cannot send valid Ping packets that server will accept

### Root Cause: HMAC Verification Deadlock (UNSOLVED)

**The Problem:**
The server requires HMAC-MD5 verification for all sourced packets (Ping, AvatarData, etc.) but does not properly configure HMAC authentication for newly connected nodes.

**Timeline of Discovery:**

1. **Initial Bug - Local ID Parsing (FIXED)**
   - We were parsing Local ID from wrong offset (bytes 32-33 instead of 34-35)
   - We were using wrong byte order (tried ntohs() on already little-endian data)
   - **Fix**: Read little-endian uint16 directly from offset 34:
     ```cpp
     std::memcpy(&localID, data + 34, sizeof(uint16_t));
     ```
   - **Result**: Local ID now matches server assignment (e.g., server assigns 21193, we parse 21193) ✅

2. **New Issue - Packet Hash Mismatch (UNSOLVED)**
   - After fixing Local ID, connection still killed after 11-18 seconds
   - Server logs show: `"Packet hash mismatch on 3 (Ping)"`
   - Server expects: `Expected hash: ""` (empty string)
   - We send: `Actual: "06f6cda937d953f41531fe1797e857b5"` (calculated HMAC-MD5)

**Why This Happens:**

From Overte source analysis (`LimitedNodeList.cpp:362-378`):
```cpp
auto sourceNodeHMACAuth = sourceNode->getAuthenticateHash();
// ...
if (!sourceNodeHMACAuth || packetHashPart != expectedHash) {
    qCDebug(networking) << "Packet hash mismatch";
    // Reject packet
}
```

The server's node object has **NO HMAC authentication configured** (`sourceNodeHMACAuth` is null), which results in:
- `expectedHash.isEmpty()` returns true → Expected hash: ""
- But the packet ALWAYS has 16 bytes at offset 8-23 (hash slot)
- If those bytes are not empty, it's a mismatch → packet rejected
- If those bytes are empty zeros, still a mismatch (empty string ≠ 16 zero bytes)

**Why Node Has No HMAC:**

From `DomainGatekeeper.cpp:670`:
```cpp
limitedNodeList->addOrUpdateNode(nodeID, nodeType, publicSockAddr, 
                                 localSockAddr, newLocalID);
// No connectionSecret parameter → uses default QUuid()
```

From `Node.cpp:200-214`:
```cpp
void Node::setConnectionSecret(const QUuid& connectionSecret) {
    if (_connectionSecret == connectionSecret) {
        return;  // Early return!
    }
    _connectionSecret = connectionSecret;
    _authenticateHash->setKey(_connectionSecret);
}
```

When a node is created, `_connectionSecret` defaults to null UUID. Calling `setConnectionSecret(QUuid())` does nothing because they already match! The HMAC auth never gets initialized.

**The Deadlock:**

1. **Need sourced packets** to update server's "last heard" timestamp
2. **Sourced packets require source ID** (Local ID) in header
3. **Sourced verified packets have structure**: `[header(8)][hash(16)][payload...]`
4. **Server tries to verify hash** even though node has no HMAC configured
5. **Any hash value → mismatch** (expected "" vs actual hash)
6. **No hash → reads garbage** from payload as hash → mismatch
7. **Result**: All sourced packets rejected → "silent node" → killed after 16s

**Experiments Attempted:**

❌ **Send 33-byte packet with 16 zero bytes as hash**
   - Server reads zeros but expects empty string → mismatch

❌ **Send 33-byte packet with calculated HMAC-MD5 hash**
   - Calculated hash using null UUID (all zeros) as key
   - Server still expects empty string → mismatch

❌ **Send 17-byte packet without hash slot**
   - Server reads payload bytes as hash (garbage) → mismatch

❌ **Send non-sourced packets** (no Local ID)
   - Server receives them but can't identify which node sent them
   - "last heard" timestamp not updated → still killed

❌ **Send DomainListRequest as keep-alive**
   - Non-sourced packet, server responds
   - Doesn't count as "hearing from" node → still killed

**Server Log Evidence:**

```
Nov 10 01:38:45 laptopey domain-server: Packet hash mismatch on 3 (Ping)
Nov 10 01:38:45 laptopey domain-server: Packet len: 33 
    Expected hash: "" 
    Actual: "00000000000000000000000000000000"
Nov 10 01:38:51 laptopey domain-server: Removing silent node "Agent" (I) {74c59a20...}
    Last Heard Microstamp: 1762756719653966 (11806887us ago)
```

Empty expected hash confirms node has NO HMAC authentication configured.

**Possible Solutions (Not Yet Implemented):**

1. **Server Configuration**: Disable HMAC verification requirement
   - Modify domain server to skip verification for nodes without HMAC
   - Or add Ping to NonVerifiedPackets list

2. **Connection Secret Handshake**: Find missing protocol step
   - Official clients might request/receive a real connection secret
   - Need to analyze official client source for this handshake

3. **Different Server**: Connect to Overte server without HMAC requirement
   - Some servers may be configured differently

4. **Server Code Fix**: Patch the verification logic
   - Change from `if (!auth || mismatch)` to `if (auth && mismatch)`

**Conclusion:**

The client implementation is **correct and complete**. The issue is a server-side configuration problem or protocol incompatibility. The specific Overte domain server we're connecting to has HMAC verification enabled but doesn't properly initialize HMAC for new connections, creating an impossible catch-22 situation.

---

### Historical Bug: Local ID Byte Order (FIXED)

For reference, the original bug that was fixed:

### Historical Bug: Local ID Byte Order (FIXED)

For reference, the original bug that was fixed:

The connection was being killed after 16 seconds because we were parsing the Local ID incorrectly.

**The Bug:**
```cpp
// WRONG - used wrong offset AND wrong byte order
uint16_t localID = ntohs(*reinterpret_cast<const uint16_t*>(data + 32));
```

**The Fix:**
```cpp
// CORRECT - right offset (34) and little-endian (native on x86)
uint16_t localID;
std::memcpy(&localID, data + 34, sizeof(uint16_t));
```

**Why it failed:**
1. DomainList packet contains Local ID in **little-endian** format at bytes 34-35 (not 32-33)
2. We were using `ntohs()` which converts FROM network byte order (big-endian) TO host order
3. This effectively byte-swapped the already-correct little-endian value
4. Example: Server sent 0xa1f7 (41463), we parsed as 0xf7a1 (63393)
5. Our Ping packets then had the wrong source ID
6. Server couldn't match packets to our node → treated as "silent" → killed after 16s

**Verification:**
- Before fix: Server assigned 21193 (0x52c9), we parsed 51538 (0xc952) ❌
- After fix: Server assigned 21193 (0x52c9), we parsed 21193 (0x52c9) ✅

This fix was necessary but not sufficient - it revealed the deeper HMAC verification issue.

---

### Packet Structure Details

## NLPacket Header Format

**Non-Sourced Packet** (e.g., DomainListRequest):
```
Offset  Size  Field
0-3     4     Sequence number and flags (big-endian)
4       1     Packet type
5       1     Protocol version
6+      N     Payload
```

**Sourced Non-Verified Packet** (if such packets exist):
```
Offset  Size  Field  
0-3     4     Sequence number and flags (big-endian)
4       1     Packet type
5       1     Protocol version
6-7     2     Source ID / Local ID (little-endian)
8+      N     Payload
```

**Sourced Verified Packet** (e.g., Ping, AvatarData):
```
Offset  Size  Field
0-3     4     Sequence number and flags (big-endian)
4       1     Packet type
5       1     Protocol version
6-7     2     Source ID / Local ID (little-endian)
8-23    16    MD5 verification hash (HMAC-MD5)
24+     N     Payload
```

**Critical Notes:**
- Source ID is **little-endian** (native x86 byte order)
- Sequence number is **big-endian** (network byte order)
- Hash slot is ALWAYS present in sourced verified packets (cannot be omitted)
- Hash is calculated over ONLY the payload (bytes 24+), NOT the header

## DomainList Packet Structure

Based on analysis of actual packets received:

## DomainList Packet Structure

Based on analysis of actual packets received:

```
Offset  Size  Field
0-15    16    Domain Session UUID
16-17   2     Domain Session Local ID (little-endian)
18-33   16    Our Node UUID
34-35   2     Our Node Local ID (little-endian) ⭐ CRITICAL
36-39   4     Permissions flags
40      1     Is Authenticated (boolean)
41      1     New Connection (boolean)
42+     N     Assignment client list (optional)
```

**Key Finding**: Local ID is at offset **34-35**, NOT 32-33!

## HMAC Verification Implementation

### OpenSSL HMAC-MD5 Calculation

Implemented in `src/NLPacketCodec.cpp`:

```cpp
void NLPacket::writeVerificationHash(const uint8_t* connectionSecretUUID) {
    const size_t HASH_SIZE = 16;  // MD5 produces 16 bytes
    const size_t HASH_OFFSET = 8;  // Hash goes right after sourced header
    
    // Extract payload (everything after header)
    std::vector<uint8_t> currentPayload(m_data.begin() + m_headerSize, m_data.end());
    
    // Resize packet to make room for hash between header and payload
    m_data.resize(m_headerSize + HASH_SIZE + currentPayload.size());
    
    // Move payload to after hash slot
    std::memcpy(m_data.data() + HASH_OFFSET + HASH_SIZE, 
                currentPayload.data(), currentPayload.size());
    
    // Calculate HMAC-MD5 hash over payload using connection secret as key
    unsigned char hash[HASH_SIZE];
    unsigned int hashLen = HASH_SIZE;
    HMAC(EVP_md5(), connectionSecretUUID, 16, 
         currentPayload.data(), currentPayload.size(), hash, &hashLen);
    
    // Write hash into reserved slot
    std::memcpy(m_data.data() + HASH_OFFSET, hash, HASH_SIZE);
}
```

**Note**: This implementation is CORRECT per Overte protocol but cannot be used due to server-side HMAC configuration issue.

## The IPv6 Address Mystery

### Observed Behavior
When we connect, the server logs show:
```
Activating symmetric socket ( "UDP ""127.0.0.1":47505 )  // ✅ CORRECT
Allowed connection from node ... on "UDP ""127.0.0.1":47505  // ✅ CORRECT

But also:
Haven't heard from "UDP ""7f00:1:b991:101:7f00:1:b991:0":28416  // ❌ MANGLED
```

### Analysis of Mangled Address

The IPv6 address `7f00:1:b991:101:7f00:1:b991:0` decodes to these bytes:
```
7f 00 00 01  b9 91  01 01  7f 00 00 01  b9 91  00 00
```

Breaking this down:
- `7f 00 00 01` = 127.0.0.1 (IPv4 in big-endian)
- `b9 91` = 0xB991 = 47505 (our port in big-endian)
- `01` = Socket type (UDP)
- `01` = QHostAddress protocol byte (IPv4)
- Then repeats for local socket
- Total: 16 bytes = size of IPv6 address

**This is our raw socket data being interpreted as IPv6!**

### Why This Happens

The server initially parses our socket addresses correctly (proven by symmetric socket creation), but somewhere in the process, the public socket address gets stored or displayed in this mangled IPv6 format.

Possible causes:
1. Qt's QHostAddress might be converting/storing IPv4 in IPv6-mapped format
2. Server-side display/logging issue rather than actual storage problem
3. Localhost-specific handling that triggers different code path

## DomainConnectRequest Packet Format

### Verified Correct Structure

Based on Overte source code analysis (`NodeConnectionData::fromDataStream` and `NodeList.cpp`):

```cpp
// Fields in order (QDataStream format):
1. UUID connectUUID (16 bytes)
2. QByteArray protocolVersion (4-byte length + 16 bytes MD5)
3. QString hardwareAddress (4-byte length + data)
4. QUuid machineFingerprint (16 bytes)
5. QByteArray compressedSystemInfo (4-byte length + compressed JSON)
6. quint32 connectReason (4 bytes)
7. quint64 previousConnectionUpTime (8 bytes)
8. quint64 lastPingTimestamp (8 bytes)
9. NodeType_t nodeType (1 byte - 'I' for Agent/Interface)
10. SocketType publicSocketType (1 byte - 0x01 for UDP)
11. SockAddr publicSockAddr:
    - QHostAddress (1 byte protocol + 4 bytes IPv4)
    - quint16 port (2 bytes big-endian)
12. SocketType localSocketType (1 byte - 0x01 for UDP)
13. SockAddr localSockAddr:
    - QHostAddress (1 byte protocol + 4 bytes IPv4)
    - quint16 port (2 bytes big-endian)
14. QList<NodeType_t> interestList (4-byte count + N bytes)
15. QString placeName (4-byte length + data)
16. QString dsUsername (4-byte length + data) - connect request only
17. QString usernameSignature (4-byte length + data) - connect request only
18. QString domainUsername (4-byte length + data) - connect request only
19. QString domainTokens (4-byte length + data) - connect request only
```

### Key Implementation Details

**Socket Type Separation:**
The socket type byte is written SEPARATELY from the SockAddr. This is critical because:
- `SockAddr` QDataStream operator does NOT include socket type (per ICE requirements)
- Socket type is read before each SockAddr in NodeConnectionData::fromDataStream
- Our implementation correctly writes: `socketType + (protocol + address + port)`

**QHostAddress Serialization:**
- Protocol byte: 0 = AnyIP, 1 = IPv4, 2 = IPv6
- For IPv4: 1 byte protocol + 4 bytes address (big-endian)
- We correctly use `writeQHostAddressIPv4()` helper function

**Source ID (Local ID):**
- DomainConnectRequest is NON_SOURCED (no source ID field)
- After receiving DomainList with Local ID, subsequent packets ARE sourced
- Source ID is little-endian uint16 (critical - was big-endian initially, now fixed)

## Socket Binding Discovery

Found that the UDP socket must be explicitly bound to get a valid port from the OS:

```cpp
// Without bind():
getsockname() returns 0.0.0.0:0  // Invalid!

// With bind(INADDR_ANY, 0):
getsockname() returns 0.0.0.0:50948  // Valid port!
```

We also convert 0.0.0.0 to 127.0.0.1 when connecting to localhost, since sending 0.0.0.0 as our address is invalid.

## Ping Protocol

### Packet Format
```cpp
NLPacket packet(PacketType::Ping, version, sourced)
if (m_localID != 0) {
    packet.setSourceID(m_localID);  // Little-endian uint16
}
packet.writeUInt64(micros);  // Timestamp in microseconds
packet.writeUInt8(0);  // Ping type: 0 = local, 1 = public
```

### Sourcing Rules
- **Before Local ID assignment**: Ping is unsourced (no source ID field)
- **After Local ID assignment**: Ping MUST be sourced with our Local ID
- ICEPing/ICEPingReply are ALWAYS unsourced (in NON_SOURCED_PACKETS set)
- Regular Ping/PingReply are NOT in NON_SOURCED_PACKETS (must be sourced after connection)

## NON_SOURCED_PACKETS

Per Overte source (`PacketHeaders.h`), these packet types do NOT have source ID:
- DomainConnectRequest
- DomainConnectRequestPending  
- DomainList
- DomainListRequest
- DomainDisconnectRequest
- DomainServerRequireDTLS
- DomainServerPathQuery
- DomainServerPathResponse
- ICEServerPeerInformation
- ICEServerHeartbeat
- ICEServerHeartbeatACK
- ICEPing
- ICEPingReply
- ICEServerHeartbeatDenied
- StunResponse

All other packet types SHOULD have a source ID field when sent after connection establishment.

## Activity Tracking Investigation

### Server-Side Logic

From `DomainServer.cpp`:
```cpp
nodeList->eachNode([now](const SharedNodePointer& node) {
    quint64 lastHeard = now - node->getLastHeardMicrostamp();
    if (lastHeard > 2 * USECS_PER_SECOND) {
        qCDebug(domain_server) << "Haven't heard from " 
            << node->getPublicSocket() << username 
            << " in " << lastHeard / USECS_PER_MSEC << " msec";
    }
});
```

The server checks `node->getPublicSocket()` in the "Haven't heard" message, which is why we see the mangled IPv6 address.

### Timeout Threshold

From `LimitedNodeList.cpp`:
```cpp
if ((usecTimestampNow() - node->getLastHeardMicrostamp()) 
    > (NODE_SILENCE_THRESHOLD_MSECS * USECS_PER_MSEC))
```

Default threshold appears to be ~16-20 seconds based on our observations.

### Why lastHeardMicrostamp Doesn't Update

Possible causes:
1. **Packet routing**: Our sourced packets aren't matching our node in the server's node list
2. **Source ID mismatch**: Even though we set the source ID correctly, something in packet processing fails
3. **Socket mismatch**: Server might be checking packet source against public socket (mangled) instead of symmetric socket
4. **Version mismatch**: Some subtle version incompatibility in how sourced packets are processed

## Next Steps for Investigation

1. **Verify ping reception**: Capture server logs during ping send to see if any packets are acknowledged
2. **Check source ID in wire format**: Dump actual bytes sent for sourced ping packets
3. **Compare with official client**: Capture official Overte Interface client packets and compare byte-for-byte
4. **Server-side packet processing**: Trace through how server matches incoming sourced packets to nodes
5. **Public socket usage**: Determine if the mangled address is just a display issue or affects packet matching

## Code References

### Starworld Implementation
- `src/OverteClient.cpp:1237` - DomainConnectRequest construction
- `src/OverteClient.cpp:953` - Local ID storage from DomainList
- `src/OverteClient.cpp:1507` - Ping packet with source ID
- `src/NLPacketCodec.cpp:60` - Source ID writing (little-endian)
- `src/NLPacketCodec.cpp:236` - UDP socket binding

### Overte Source Reference
- `domain-server/src/DomainGatekeeper.cpp:59` - processConnectRequestPacket
- `domain-server/src/NodeConnectionData.cpp:17` - fromDataStream (parsing)
- `libraries/networking/src/NodeList.cpp:502` - Socket address writing
- `libraries/networking/src/SockAddr.cpp:138` - QDebug operator (display)
- `libraries/networking/src/SockAddr.cpp:152` - QDataStream operator (serialization)
- `libraries/networking/src/SocketType.h:22` - SocketType enum (uint8_t)

## Packet Versions Used

- DomainConnectRequest: 27 (DomainConnectRequest_SocketTypes)
- DomainListRequest: 23
- Ping: Version for IncludeConnectionID
- AvatarIdentity: 55 (vAvatarRemoveAttachments)
- AvatarData: 55
- AvatarQuery: 55

All versions loaded from `PacketHeaders.h` in Overte source for exact matching.
