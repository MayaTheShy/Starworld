# Starworld (StardustXR + Overte Client)

[![CI](https://git.spatulaa.com/MayaTheShy/Starworld/actions/workflows/ci.yml/badge.svg)](https://git.spatulaa.com/MayaTheShy/Starworld/actions)

## Overview

Starworld is an [Overte](https://overte.org) client that renders virtual world entities inside the [StardustXR](https://stardustxr.org) compositor. It bridges Overte's entity protocol with Stardust's spatial computing environment, allowing you to view and interact with Overte domains in XR.

**Current Status:** ✅ **Connection persistence is now fixed. All core entity rendering features are implemented. Color tinting and texture application are pending StardustXR API support.**

✨ **Working Features:**
- Complete DomainConnectRequest implementation with OAuth authentication
- Local ID assignment and parsing (fixed byte order bugs)
- 3D model rendering with HTTP asset downloading
- ModelCache automatically downloads models from http:// and https:// URLs to `~/.cache/starworld/models/`
- Primitive models (cube, sphere, suzanne) pre-generated in `~/.cache/starworld/primitives/`
- HMAC-MD5 packet verification implementation (correct but blocked by server config)

ℹ️ **Note:**
Connection persistence is now fixed (see below). For protocol details, see [`docs/NETWORK_PROTOCOL_INVESTIGATION.md`](docs/NETWORK_PROTOCOL_INVESTIGATION.md). For troubleshooting, see [`docs/ENTITY_TROUBLESHOOTING.md`](docs/ENTITY_TROUBLESHOOTING.md).

### About the Technologies

**[StardustXR](https://stardustxr.org)** is a next-generation XR display server and compositor that runs XR clients as separate processes. It provides a Unix-philosophy approach to XR, where each application is an independent process communicating via IPC.
- **Website**: https://stardustxr.org
- **GitHub**: https://github.com/StardustXR
- **Core Repository**: https://github.com/StardustXR/core
- **Server**: https://github.com/StardustXR/server
- **Documentation**: https://stardustxr.org/docs

**[Overte](https://overte.org)** is an open-source virtual worlds and social VR platform, a community-maintained fork of High Fidelity. It allows you to create and explore 3D virtual environments with others.
- **Website**: https://overte.org
- **GitHub**: https://github.com/overte-org/overte
- **Documentation**: https://docs.overte.org
- **API Reference**: https://apidocs.overte.org
- **Discord**: https://discord.gg/overte
- **Metaverse**: https://mv.overte.org (main public metaverse server)

## Quick Start

### Prerequisites
- CMake 3.15+
- C++20 compiler (GCC/Clang)
- Rust toolchain (for the bridge)
- **StardustXR server running** (required!)
- Required libraries: glm, OpenSSL, zlib, libcurl

**Important**: The application will exit if no StardustXR compositor is detected. Make sure to start the Stardust server first:

```bash
# Start StardustXR server (in a separate terminal)
stardust-xr-server
```

### Build Everything
```bash
./scripts/build_and_test.sh
```

Or manually:

```bash
# 1. Build Rust bridge
cd bridge
cargo build --release
cd ..

# 2. Build C++ client
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run with Simulation Mode
Test the rendering without connecting to an Overte server:

```bash
export STARWORLD_SIMULATE=1
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/starworld
```

This creates three demo entities rendered as 3D models:
- **Red cube** (0.2m) - Box entity type
- **Green sphere** (0.15m) - Sphere entity type
- **Blue suzanne** (0.25m) - Model entity type (Blender monkey head placeholder)

### Connect to Overte Server

Connect to a domain using the domain address format:

```bash
# Using domain:port format (port is UDP domain server port)
./build/starworld --overte=127.0.0.1:40104

# Using domain address with position/orientation (position/rotation ignored for now)
./build/starworld --overte=142.122.4.245:40104/0,0,0/0,0,0,1

# Using WebSocket URL format (deprecated, but still works)
./build/starworld --overte=ws://domain.example.com:40102
```

**Address Format:**
- `host:40104` - Connects to UDP domain server on port 40104 (standard Overte port)
- HTTP port is automatically calculated as UDP port - 2 (e.g., 40102 for UDP 40104)
- Position/orientation coordinates after `/` are currently ignored

### Connect with Authentication

**✨ OAuth Browser Authentication Now Implemented!**

Starworld now supports full OAuth 2.0 authentication via browser flow (Authorization Code Grant). This allows you to authenticate with your Overte account and access private domains and full entity data.

**Quick Start - Browser OAuth (Recommended):**
```bash
# Automatic browser-based login
./build/starworld --auth --overte=127.0.0.1:40102

# The application will:
# 1. Start a local callback server (usually port 8765)
# 2. Open your web browser to the Overte login page
# 3. Wait for you to log in
# 4. Receive the authorization code
# 5. Exchange it for an access token
# 6. Save the token for future use
```

**Features:**
- ✅ Browser-based OAuth 2.0 (Authorization Code Grant)
- ✅ Automatic token refresh
- ✅ Token persistence (`~/.config/starworld/overte_token.txt`)
- ✅ CSRF protection with state parameter
- ✅ Secure local callback server (localhost only)
- ✅ Fallback to saved tokens
- ✅ Username/password login (less secure, for testing)

**Advanced Options:**
```bash
# Use saved token if available, otherwise open browser
./build/starworld --auth

# Specify metaverse server
OVERTE_METAVERSE=https://mv.overte.org ./build/starworld --auth

# Legacy username/password (NOT RECOMMENDED - use browser flow)
./build/starworld --auth --username=myuser --password=mypass

# Force re-authentication (deletes saved token)
rm ~/.config/starworld/overte_token.txt && ./build/starworld --auth
```

**How It Works:**
1. Application starts HTTP callback server on `http://localhost:8765/callback`
2. Opens browser to: `https://mv.overte.org/oauth/authorize?...`
3. User logs in via Overte's web interface
4. Overte redirects to `http://localhost:8765/callback?code=ABC&state=XYZ`
5. Application receives authorization code
6. Exchanges code for access token via POST to `/oauth/token`
7. Saves token to `~/.config/starworld/overte_token.txt`
8. Token is automatically refreshed when expiring

**Benefits of Authenticated Connection:**
- Access to private/restricted domains
- Full entity server topology information
- Direct EntityServer connections (faster, more reliable)
- User profile information
- Permission to edit entities
- Voice chat capabilities (future)

**Anonymous Connection (No --auth flag):**
```bash
./build/starworld --overte=127.0.0.1:40104
```

Anonymous users can:
- Connect to public domains
- Query entity data (limited by server permissions)
- Receive domain list packets
- View and render entities (if server allows)

Limitations:
- No assignment client topology information
- EntityServer address not advertised (uses domain server fallback)
- Some restricted domains may reject anonymous connections
- Cannot edit entities or participate in voice chat

### Domain Discovery

```
Overte Server (UDP) → OverteClient (C++) → SceneSync → StardustBridge (C++) 
                                                            ↓ (dlopen C-ABI)
                                                       Rust Bridge
                                                            ↓
                                                    Stardust Server
```

The Rust bridge provides the StardustXR client implementation, exposing a C ABI for the C++ code to use.

## Entity Rendering

Starworld renders Overte entities as **3D GLTF/GLB models**:

- **Box** (type 1): Cube model from `cube.glb`
- **Sphere** (type 2): Sphere model from `sphere.glb`
- **Model** (type 3): Suzanne (Blender monkey) from `model.glb`, or downloaded models
- **Other types**: Coming soon (Text, Image, Light, etc.)

**Current Support:**
- ✅ Position, rotation, scale (full transform matrix)
- ✅ Dimensions (xyz size in meters)
- ✅ GLTF/GLB model loading from local cache
- ✅ HTTP/HTTPS model URL downloading with ModelCache (SHA256-based caching)
- ✅ Primitive generation using Blender (`tools/blender_export_simple.py`)
- ⏳ Entity colors (stored but not yet applied to models)
- ⏳ Texture support (entity.textureUrl parsing implemented)
- ⏳ ATP protocol support (Overte asset server)

**Cache Structure:**
- Downloaded models: `~/.cache/starworld/models/` (SHA256 URL hashing)
- Primitive models: `~/.cache/starworld/primitives/` (Blender-generated)
- HTTP downloads use libcurl with async callbacks and progress reporting

## Rust Bridge

The bridge (required for proper StardustXR integration) is a Rust shared library that:
- Connects to the Stardust compositor via fusion client
- Manages the spatial scene graph
- Handles entity creation, updates, and removal
- Renders entities using the asteroids element API

Build it with:
```bash
cd bridge
cargo build --release
```

This produces `bridge/target/release/libstardust_bridge.so`, which the C++ client loads at runtime.

### Bridge Path Configuration
The client tries these locations in order:
1. `STARWORLD_BRIDGE_PATH` environment variable
2. `./bridge/target/debug/libstardust_bridge.so`
3. `libstardust_bridge.so` (system library path)

## Configuration Options

### Environment Variables
- `STARWORLD_BRIDGE_PATH`: Path to bridge .so directory
- `STARWORLD_SIMULATE`: Set to `1` for simulation mode (no Overte connection)
- `STARDUSTXR_SOCKET`: Override Stardust compositor socket path
- `OVERTE_URL`: Override Overte server URL (deprecated, use --overte flag)
- `OVERTE_UDP_PORT`: Override UDP domain server port (default: from URL or 40104)
- `OVERTE_DISCOVER`: Enable domain discovery (`1` or `true`)
- `OVERTE_DISCOVER_PROBE`: Enable/disable domain reachability probing
- `OVERTE_DISCOVER_INDEX`: Manual domain selection index
- `OVERTE_USERNAME`: Reserved for future OAuth (currently unused)
- `OVERTE_PASSWORD`: Reserved for future OAuth (currently unused)
- `OVERTE_METAVERSE`: Reserved for future OAuth (currently unused)

### Command-Line Options
- `--socket=/path/to.sock`: Override Stardust socket path
- `--abstract=name`: Use abstract socket namespace
- `--overte=host:port`: Connect to Overte domain (port is UDP port, typically 40104)
- `--overte=host:port/x,y,z/qx,qy,qz,qw`: Domain address with spawn position (position ignored)
- `--discover`: Enable Overte domain discovery via metaverse directories

## Development

### Project Structure
```
Starworld/
├── src/              # C++ source files
│   ├── main.cpp
│   ├── StardustBridge.cpp
│   ├── OverteClient.cpp
│   ├── SceneSync.cpp
│   └── ...
├── bridge/           # Rust bridge
│   ├── src/lib.rs
│   └── Cargo.toml
├── tests/            # Test harness
├── tools/            # Python utilities
├── scripts/          # Build and utility scripts
├── docs/             # Documentation files
└── examples/         # Example configurations and models
```

### Debugging
Enable verbose logging:
```bash
export RUST_LOG=debug
export LOG_LEVEL=debug
./build/starworld
```

### Vendoring StardustXR Crates
For deterministic builds, clone dependencies into `third_party/`:

```bash
cd third_party
git clone https://github.com/StardustXR/asteroids.git
git clone https://github.com/StardustXR/core.git
```

Then update `bridge/Cargo.toml`:
```toml
[dependencies.stardust-xr-asteroids]
path = "../third_party/asteroids"

[dependencies.stardust-xr-fusion]
path = "../third_party/core"
```

This allows you to:
- Lock to specific commits
- Patch client internals
- Debug client crate issues
- Add custom C ABI exports

## Known Limitations

1. **Color Tinting Not Visually Applied**: Color data is parsed, stored, and logged, but not yet applied to model materials (requires StardustXR asteroids API extension). See [`docs/ENTITY_TROUBLESHOOTING.md`](docs/ENTITY_TROUBLESHOOTING.md).

2. **Texture Application Not Implemented**: Texture URLs are parsed, textures are downloaded and cached, but not yet visually applied to models (requires StardustXR material API).

3. **ATP Protocol Not Supported**: atp:// asset protocol is not yet supported (requires AssetClient integration). Use HTTP URLs for now.

4. **Entity Types**: Only Box, Sphere, Model are supported. Text, Image, Light, Zone, etc. are not yet implemented.

5. **Limited Entity Updates**: Entities are created, but real-time updates and deletions are not fully supported.

6. **Single User**: No avatar or multi-user support yet.

7. **NAT/Firewall**: External connections require port forwarding for self-hosted domains.

## Roadmap

### Phase 1: Core Rendering ✅ COMPLETE
- [x] Entity type differentiation
- [x] **3D model rendering with GLTF/GLB** 🎉
- [x] Transform support (position, rotation, scale)
- [x] Dimension support (xyz sizing)

### Phase 2: Asset Pipeline (In Progress)
- [x] Local asset cache (`~/.cache/starworld/primitives/`)
- [x] Blender primitive generation (`tools/blender_export_simple.py`)
- [x] **HTTP model downloader with ModelCache** 🎉
- [x] Download models from entity.modelUrl (http/https)
- [x] SHA256-based caching with libcurl
- [x] Async download callbacks with progress
- [x] Texture download and caching (infrastructure complete)
- [ ] ATP protocol support (Overte asset server)
- [ ] Material color application to models (pending API)
- [ ] Texture loading and mapping (pending API)

### Phase 3: Network & Protocol ✅ COMPLETE
- [x] Domain connection via UDP
- [x] NLPacket protocol implementation
- [x] DomainConnectRequest / DomainList handshake
- [x] QDataStream parsing for Overte packets
- [x] Assignment client list parsing
- [x] EntityQuery packet implementation
- [x] Session UUID generation
- [x] Protocol signature verification (MD5)
- [x] Domain address parsing (host:port/position/orientation)
- [x] **OAuth 2.0 authentication with browser flow** 🎉
- [x] Token persistence and refresh
- [x] Connection persistence bug fixed (see docs)
- [ ] Assignment client direct connections
- [ ] Authenticated EntityServer queries

### Phase 4: Entity System (Current Focus)
- [ ] Apply entity colors to model materials
- [ ] All entity types (Text, Image, Light, Zone, etc.)
- [ ] Entity property updates (real-time position, rotation, color changes)
- [ ] Entity deletion handling
- [ ] Parent/child entity hierarchies
- [ ] Entity query/filtering by distance

### Phase 5: Interaction & Multi-User
- [ ] Avatar representation and sync
- [ ] Input forwarding (XR controllers → Overte)
- [ ] Audio spatial rendering
- [ ] Voice chat integration

### Phase 6: Advanced Features
- [ ] Script integration
- [ ] Physics simulation
- [ ] Particle effects
- [ ] Animation playback
- [ ] Material/texture overrides

## Troubleshooting

See [`docs/ENTITY_TROUBLESHOOTING.md`](docs/ENTITY_TROUBLESHOOTING.md) for a complete troubleshooting guide, debug flags, and common issues.

## Contributing

### Documentation
- **[README.md](README.md)** - Main documentation (you are here)
- **[docs/DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md)** - Quick reference for developers
- **[docs/CHANGELOG.md](docs/CHANGELOG.md)** - Version history and changes
- **[docs/OVERTE_AUTH.md](docs/OVERTE_AUTH.md)** - OAuth implementation details
- **[docs/OVERTE_ASSIGNMENT_CLIENT_TASK.md](docs/OVERTE_ASSIGNMENT_CLIENT_TASK.md)** - Protocol implementation
- **[docs/ENTITY_RENDERING_ENHANCEMENTS.md](docs/ENTITY_RENDERING_ENHANCEMENTS.md)** - Rendering implementation
- **[docs/MODELCACHE_IMPLEMENTATION.md](docs/MODELCACHE_IMPLEMENTATION.md)** - Asset pipeline details
- **[docs/CI_SETUP_SUMMARY.md](docs/CI_SETUP_SUMMARY.md)** - Continuous integration setup

### Getting Started
1. Read [docs/DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md) for build/run commands
2. Check [docs/CHANGELOG.md](docs/CHANGELOG.md) for recent changes
3. Review protocol details in [docs/OVERTE_ASSIGNMENT_CLIENT_TASK.md](docs/OVERTE_ASSIGNMENT_CLIENT_TASK.md)

### Development Workflow
1. Create feature branch: `git checkout -b feature/my-feature`
2. Make changes and test locally: `./scripts/ci-test.sh`
3. Commit with clear messages
4. Push and create PR in Gitea
5. CI will run automated tests

See [docs/CI_SETUP_SUMMARY.md](docs/CI_SETUP_SUMMARY.md) for details on the CI pipeline.

## References & Resources

See the documentation in the `docs/` directory for protocol, troubleshooting, and implementation details. For StardustXR and Overte resources, see:

- **StardustXR**: https://stardustxr.org, https://github.com/StardustXR
- **Overte**: https://overte.org, https://github.com/overte-org/overte
- **GLTF/GLB Format**: https://www.khronos.org/gltf/
- **Blender**: https://www.blender.org
