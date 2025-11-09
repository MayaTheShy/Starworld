# Starworld (StardustXR + Overte Client)

[![CI](https://gitea.example.com/yourusername/starworld/actions/workflows/ci.yml/badge.svg)](https://gitea.example.com/yourusername/starworld/actions)

## Overview

Starworld is an Overte client that renders virtual world entities inside the StardustXR compositor. It bridges Overte's entity protocol with Stardust's spatial computing environment, allowing you to view and interact with Overte domains in XR.

**Current Status:** ✨ **3D model rendering with HTTP asset downloading!** Entities render as GLTF/GLB models loaded from the local cache. ModelCache automatically downloads models from http:// and https:// URLs to `~/.cache/starworld/models/`. Primitive models (cube, sphere, suzanne) are pre-generated in `~/.cache/starworld/primitives/` using Blender.

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
```bash
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/starworld ws://domain.example.com:40102
```

Or use domain discovery:
```bash
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/starworld --discover
```

## Architecture

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
- `OVERTE_URL`: Override Overte server URL
- `OVERTE_DISCOVER`: Enable domain discovery (`1` or `true`)
- `OVERTE_DISCOVER_PROBE`: Enable/disable domain reachability probing
- `OVERTE_DISCOVER_INDEX`: Manual domain selection index
- `OVERTE_USERNAME`: Set username for Overte authentication

### Command-Line Options
- `--socket=/path/to.sock`: Legacy socket override
- `--abstract=name`: Use abstract socket namespace
- `--overte=ws://host:port`: Overte server WebSocket URL
- `--discover`: Enable Overte domain discovery

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

1. **Entity types**: Only Box, Sphere, Model supported. Need Text, Image, Light, Zone, etc.
2. **Model colors**: Entity colors are parsed but not yet applied to materials
3. **No texture support**: Models render without textures, entity.textureUrl parsing ready
4. **Limited entity updates**: Entities created but updates/deletions need work
5. **UDP only**: WebSocket transport not implemented
6. **Single user**: No avatar or multi-user support yet

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

### Phase 3: Entity System (Current Focus)
- [ ] Apply entity colors to model materials ⏭️ NEXT
- [ ] All entity types (Text, Image, Light, Zone, etc.)
- [ ] Entity property updates (real-time position, rotation, color changes)
- [ ] Entity deletion handling
- [ ] Parent/child entity hierarchies
- [ ] Entity query/filtering by distance

### Phase 4: Interaction & Multi-User
- [ ] Avatar representation and sync
- [ ] Input forwarding (XR controllers → Overte)
- [ ] Audio spatial rendering
- [ ] Voice chat integration

### Phase 5: Advanced Features
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
- Verify server URL/port
- Check network connectivity
- Try `--discover` to find public domains
- Use simulation mode: `export STARWORLD_SIMULATE=1`

### Nothing renders in VR
- Check Stardust server logs for errors
- Verify entities have non-zero dimensions
- Enable debug logging: `RUST_LOG=debug`
- Look for "[bridge/reify]" log messages

## Contributing

See [ENTITY_RENDERING_ENHANCEMENTS.md](ENTITY_RENDERING_ENHANCEMENTS.md) for implementation details.

For CI/test setup, see [CI_SETUP_SUMMARY.md](CI_SETUP_SUMMARY.md).

## License

[Add your license here]
