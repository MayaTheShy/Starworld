# Changelog

All notable changes to Starworld will be documented in this file.

## [Unreleased]

### Added - November 10, 2025
- **HMAC Verification Implementation**
  - Complete HMAC-MD5 packet signing using OpenSSL
  - Verification hash calculation with connection secret UUID as key
  - Proper hash slot insertion in sourced packet structure
  - writeVerificationHash() method for NLPacket class
  - See NETWORK_PROTOCOL_INVESTIGATION.md for detailed analysis

- **Local ID Parsing Fix**
  - Fixed byte order bug: Local ID is little-endian, not big-endian
  - Fixed offset bug: Local ID at bytes 34-35, not 32-33 in DomainList
  - Source ID now correctly matches server assignment

- **Protocol Debugging**
  - Comprehensive packet hex dumping for analysis
  - Server log correlation with client packets
  - Detailed HMAC verification failure investigation

### Added - November 2025
- **Overte Protocol Implementation**
  - Complete NLPacket protocol support for Overte domains
  - DomainConnectRequest / DomainList handshake implementation
  - QDataStream serialization compatible with Qt format
  - Assignment client discovery from DomainList packets
  - Session UUID generation and management
  - Protocol signature verification (MD5)
  - Keep-alive ping mechanism
  - Entity query targeting with fallback support

- **Domain Connection**
  - UDP domain server connection on port 40104
  - Domain address parsing (host:port/position/orientation format)
  - Anonymous connection mode (fully functional)
  - Local ID assignment from domain server
  - Connection retry logic with exponential backoff

- **Network Features**
  - NLPacket encoder/decoder for Overte wire protocol
  - QDataStream encoder for Qt-compatible serialization
  - BigEndian integer handling for network packets
  - Packet sequence numbering
  - Source ID management for multi-client scenarios

- **OAuth Infrastructure** (disabled, needs completion)
  - OverteAuth class for OAuth 2.0 client
  - Token storage (access_token, refresh_token, expires_at)
  - Login/logout methods (framework ready)
  - See OVERTE_AUTH.md for implementation details

- **Documentation**
  - NETWORK_PROTOCOL_INVESTIGATION.md - Comprehensive protocol analysis
  - OVERTE_AUTH.md - Comprehensive OAuth implementation guide
  - OVERTE_ASSIGNMENT_CLIENT_TASK.md - Protocol implementation details
  - Updated README.md with connection instructions
  - Protocol packet format documentation

### Changed
- Updated domain connection to use UDP port format (host:40104) instead of WebSocket URLs
- HTTP port auto-calculated as UDP port - 2 (e.g., 40102 for UDP 40104)
- Disabled OAuth login attempt (needs browser-based authorization code flow)
- Entity queries sent to domain server when no EntityServer advertised

### Known Issues
- **HMAC Verification Deadlock**: Connection killed after 11-18 seconds
  - Server requires HMAC verification for sourced packets (Ping, AvatarData)
  - Server does not initialize HMAC for new nodes (expects empty hash)
  - Any hash value (even zeros) causes mismatch and packet rejection
  - Cannot send non-sourced packets for keep-alive (don't update "last heard")
  - **Root cause**: Server-side configuration issue or bug
  - **Status**: Client implementation correct; blocked by server config
  - See NETWORK_PROTOCOL_INVESTIGATION.md for full analysis

### Fixed
- Local ID byte order: now correctly reads little-endian uint16
- Local ID offset: now correctly reads from bytes 34-35 in DomainList
- Source ID in Ping packets: now matches server assignment
- Domain handshake retry loop when username sent in DomainConnectRequest
- Removed username field from anonymous connections (field 14)
- Added missing #include <endian.h> for be64toh()
- Fixed incomplete type error with unique_ptr<OverteAuth> by moving destructor to .cpp
- Rebuilt Rust bridge to use actual 3D models instead of wireframes

### Technical Details
- **DomainConnectRequest Packet**: 225 bytes for anonymous, 245 bytes with username
- **Assignment Client Security**: Only advertised to authenticated users
- **Fallback Behavior**: EntityQuery sent to domain server when no EntityServer available
- **Protocol Signature**: eb1600e798dc5e03c755a968dc16b7fc (MD5 of version string)

## [0.2.0] - 2024-11 (Previous Release)

### Added
- 3D model rendering with GLTF/GLB support
- HTTP/HTTPS model downloader with ModelCache
- SHA256-based asset caching
- Blender primitive generation (cube, sphere, suzanne)
- Entity type differentiation (Box, Sphere, Model)
- Transform support (position, rotation, scale)
- Dimension support (xyz sizing)
- Simulation mode with demo entities

### Infrastructure
- Rust bridge for StardustXR integration
- C ABI interface between C++ and Rust
- Scene graph management via asteroids API
- Build system with CMake and Cargo integration
- CI/CD with Gitea workflows

## [0.1.0] - Initial Release

### Added
- Basic StardustXR client skeleton
- Overte connection framework
- Project structure and build system
- Initial README and documentation

---

## Version History

- **Unreleased**: Overte protocol implementation, anonymous connection mode
- **0.2.0**: 3D model rendering, asset pipeline, simulation mode
- **0.1.0**: Initial project setup

## Migration Guide

### From WebSocket URLs to UDP Addresses
**Old format:**
```bash
./build/starworld --overte=ws://domain.example.com:40102
```

**New format:**
```bash
./build/starworld --overte=domain.example.com:40104
```

The port should now be the **UDP domain server port** (typically 40104), not the HTTP port.

### Authentication Changes
OAuth authentication is not yet functional. All connections are currently anonymous.

**Before:**
```bash
export OVERTE_USERNAME=user
export OVERTE_PASSWORD=pass
./build/starworld --overte=...
```

**Now:**
```bash
# Just connect (anonymous mode)
./build/starworld --overte=127.0.0.1:40104
```

See OVERTE_AUTH.md for future OAuth implementation plans.
