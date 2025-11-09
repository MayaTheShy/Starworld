# Starworld (StardustXR + Overte Client)

[![CI](https://git.spatulaa.com/MayaTheShy/Starworld/actions/workflows/ci.yml/badge.svg)](https://git.spatulaa.com/MayaTheShy/Starworld/actions)

## Overview

Starworld is an [Overte](https://overte.org) client that renders virtual world entities inside the [StardustXR](https://stardustxr.org) compositor. It bridges Overte's entity protocol with Stardust's spatial computing environment, allowing you to view and interact with Overte domains in XR.

**Current Status:** ✨ **3D model rendering with HTTP asset downloading!** Entities render as GLTF/GLB models loaded from the local cache. ModelCache automatically downloads models from http:// and https:// URLs to `~/.cache/starworld/models/`. Primitive models (cube, sphere, suzanne) are pre-generated in `~/.cache/starworld/primitives/` using Blender.

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
- StardustXR server running
- Required libraries: glm, OpenSSL, zlib, libcurl

### Build Everything
```bash
./build_and_test.sh
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

**⚠️ OAuth Not Yet Implemented** - See [OVERTE_AUTH.md](OVERTE_AUTH.md) for details.

The authentication infrastructure exists but is currently disabled. Overte uses browser-based OAuth 2.0 which requires:
- HTTP callback server for authorization code flow
- Browser launcher for login page
- Token persistence and refresh

**Current Status:**
- ✅ Anonymous connection works perfectly
- ✅ Domain connection and entity queries functional
- ❌ OAuth login disabled (needs authorization code flow implementation)
- ❌ Assignment client discovery limited to authenticated users

**Workaround:** Run in anonymous mode (default):
```bash
./build/starworld --overte=127.0.0.1:40104
```

Anonymous users can:
- Connect to public domains
- Query entity data
- Receive domain list packets
- View and render entities

Limitations:
- No assignment client topology information
- EntityServer address not advertised (uses domain server fallback)
- Some restricted domains may reject anonymous connections

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
└── tools/            # Python utilities
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

1. **OAuth Authentication**: Web-based OAuth flow not yet implemented (see OVERTE_AUTH.md)
   - Anonymous connection works perfectly
   - Assignment client discovery limited without authentication
   - Domain server used as fallback for entity queries
   
2. **Entity types**: Only Box, Sphere, Model supported. Need Text, Image, Light, Zone, etc.

3. **Model colors**: Entity colors are parsed but not yet applied to materials

4. **No texture support**: Models render without textures, entity.textureUrl parsing ready

5. **Limited entity updates**: Entities created but real-time updates/deletions need work

6. **UDP only**: All communication via UDP (HTTP used for diagnostics only)

7. **Single user**: No avatar or multi-user support yet

8. **NAT/Firewall**: External connections require port forwarding for self-hosted domains

## Roadmap

### Phase 1: Core Rendering ✅ COMPLETE
- [x] Entity type differentiation
- [x] **3D model rendering with GLTF/GLB** 🎉
- [x] Transform support (position, rotation, scale)
- [x] Dimension support (xyz sizing)

### Phase 2: Asset Pipeline ✅ COMPLETE
- [x] Local asset cache (`~/.cache/starworld/primitives/`)
- [x] Blender primitive generation (`tools/blender_export_simple.py`)
- [x] **HTTP model downloader with ModelCache** 🎉
- [x] Download models from entity.modelUrl (http/https)
- [x] SHA256-based caching with libcurl
- [x] Async download callbacks with progress
- [ ] ATP protocol support (Overte asset server)
- [ ] Material color application to models
- [ ] Texture loading and mapping

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
- [ ] OAuth 2.0 authentication (infrastructure ready, needs web flow) ⏭️ NEXT
- [ ] Assignment client direct connections
- [ ] Token persistence and refresh

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

### "Failed to connect to StardustXR compositor"
- Ensure Stardust server is running
- Check `STARDUSTXR_SOCKET` environment variable
- Try: `ss -lx | grep stardust` to find the socket

### "Rust bridge present but start() failed"
- Rebuild the bridge: `cd bridge && cargo build --release`
- Check library exists: `ls -lh bridge/target/release/libstardust_bridge.so`
- Verify RPATH: `ldd build/starworld`

### "Could not connect to Overte"
- Verify domain server is running: `ps aux | grep domain-server`
- Check UDP port: `sudo ss -ulnp | grep 40104`
- Verify network connectivity: `ping <domain-host>`
- For remote domains, check firewall/NAT port forwarding
- Try local domain first: `--overte=127.0.0.1:40104`
- Use simulation mode: `export STARWORLD_SIMULATE=1`
- Check domain server logs for connection attempts

### Nothing renders in VR
- Check Stardust server logs for errors
- Verify entities have non-zero dimensions
- Enable debug logging: `RUST_LOG=debug`
- Look for "[bridge/reify]" log messages

## Contributing

### Documentation
- **[README.md](README.md)** - Main documentation (you are here)
- **[DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)** - Quick reference for developers
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and changes
- **[OVERTE_AUTH.md](OVERTE_AUTH.md)** - OAuth implementation details
- **[OVERTE_ASSIGNMENT_CLIENT_TASK.md](OVERTE_ASSIGNMENT_CLIENT_TASK.md)** - Protocol implementation
- **[ENTITY_RENDERING_ENHANCEMENTS.md](ENTITY_RENDERING_ENHANCEMENTS.md)** - Rendering implementation
- **[MODELCACHE_IMPLEMENTATION.md](MODELCACHE_IMPLEMENTATION.md)** - Asset pipeline details
- **[CI_SETUP_SUMMARY.md](CI_SETUP_SUMMARY.md)** - Continuous integration setup

### Getting Started
1. Read [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) for build/run commands
2. Check [CHANGELOG.md](CHANGELOG.md) for recent changes
3. Review protocol details in [OVERTE_ASSIGNMENT_CLIENT_TASK.md](OVERTE_ASSIGNMENT_CLIENT_TASK.md)

### Development Workflow
1. Create feature branch: `git checkout -b feature/my-feature`
2. Make changes and test locally: `./ci-test.sh`
3. Commit with clear messages
4. Push and create PR in Gitea
5. CI will run automated tests

See [CI_SETUP_SUMMARY.md](CI_SETUP_SUMMARY.md) for details on the CI pipeline.

## License

[Add your license here]

## References & Resources

### StardustXR
- **Official Website**: https://stardustxr.org
- **GitHub Organization**: https://github.com/StardustXR
- **Core Library (Fusion)**: https://github.com/StardustXR/core
- **Server**: https://github.com/StardustXR/server
- **Asteroids (UI Elements)**: https://github.com/StardustXR/asteroids
- **Documentation**: https://stardustxr.org/docs
- **Matrix Chat**: https://matrix.to/#/#stardustxr:matrix.org

### Overte
- **Official Website**: https://overte.org
- **GitHub Repository**: https://github.com/overte-org/overte
- **User Documentation**: https://docs.overte.org
- **Developer Docs**: https://docs.overte.org/developer
- **API Reference**: https://apidocs.overte.org
- **Discord Community**: https://discord.gg/overte
- **Main Metaverse**: https://mv.overte.org
- **Protocol Documentation**: https://github.com/overte-org/overte/tree/master/libraries/networking

### Related Projects
- **High Fidelity** (Overte's predecessor): https://github.com/highfidelity/hifi (archived)
- **GLTF/GLB Format**: https://www.khronos.org/gltf/
- **Blender**: https://www.blender.org (used for primitive generation)

### Technical References
- **Qt QDataStream**: https://doc.qt.io/qt-5/qdatastream.html (Overte serialization format)
- **OAuth 2.0 RFC**: https://www.rfc-editor.org/rfc/rfc6749.html
- **NLPacket Protocol**: Documented in Overte source at `libraries/networking/src/NLPacket.h`
- **libcurl Documentation**: https://curl.se/libcurl/
