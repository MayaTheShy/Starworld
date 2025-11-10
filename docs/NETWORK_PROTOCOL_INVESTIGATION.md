# Overte Network Protocol Investigation

## Current Status (Nov 10, 2025)

### Working ✅
- **DomainConnectRequest packet format** - Matches official Overte client implementation
- **Connection established** - Server responds with DomainList packet
- **Local ID assignment** - Server assigns a valid Local ID (e.g., 166 / 0xA6)
- **Symmetric socket creation** - Server creates correct socket to our actual address (e.g., `127.0.0.1:47505`)
- **Packet version negotiation** - Using correct version 27 (DomainConnectRequest_SocketTypes)
- **Protocol signature** - MD5 hash `eb1600e798dc5e03c755a968dc16b7fc` matches server
- **Packet structure** - All fields properly ordered and serialized per Qt QDataStream format
- **Source ID handling** - Little-endian uint16, correctly set on sourced packets after receiving Local ID

### Not Working ❌
- **Connection persistence** - Connection killed after ~16 seconds with "Removing silent node"
- **Public socket storage** - Server stores mangled IPv6 address instead of IPv4
- **Activity tracking** - Server's lastHeardMicrostamp never updates despite our pings

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
