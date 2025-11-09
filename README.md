# Starworld (StardustXR + Overte Client)

[![CI](https://gitea.example.com/yourusername/starworld/actions/workflows/ci.yml/badge.svg)](https://gitea.example.com/yourusername/starworld/actions)

## Overview

Starworld is an Overte client that renders virtual world entities inside the StardustXR compositor. It bridges Overte's entity protocol with Stardust's spatial computing environment, allowing you to view and interact with Overte domains in XR.

**Current Status:** ✨ **3D colored model rendering** with full transform, color, and dimension support! Entities render as solid GLTF models with PBR materials. See [RENDERING_FIX_SUMMARY.md](RENDERING_FIX_SUMMARY.md) for implementation details.

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
./build/stardust-overte-client
```

This creates three demo entities rendered as colored 3D models:
- **Red cube** (0.2m) - smooth shaded cube with PBR material
- **Green sphere** (0.15m) - UV sphere with 32 segments
- **Blue icosphere** (0.25m) - Geodesic sphere placeholder for Model entities

### Connect to Overte Server
```bash
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/stardust-overte-client ws://domain.example.com:40102
```

Or use domain discovery:
```bash
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/stardust-overte-client --discover
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

Starworld renders Overte entities as **3D colored models**:

- **Box** (type 1): Smooth-shaded cube with colored PBR material
- **Sphere** (type 2): UV sphere (32 segments) with colored PBR material
- **Model** (type 3): Icosphere placeholder (downloads coming soon)
- **Other types**: Coming soon (Text, Image, Light, etc.)

**Current Support:**
- ✅ Position, rotation, scale (full transform matrix)
- ✅ RGB color with PBR materials (roughness, metallic)
- ✅ Dimensions (xyz size in meters)
- ✅ GLTF/GLB model loading from local cache
- ⏳ Model URL downloading (in progress)
- ⏳ Texture application (planned)

Models are loaded from `~/.cache/starworld/primitives/` with fallback to built-in primitives. Generated using Blender with `tools/blender_export_simple.py`.

For implementation details, see [ENTITY_RENDERING_ENHANCEMENTS.md](ENTITY_RENDERING_ENHANCEMENTS.md).

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
./build/stardust-overte-client
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

1. **Wireframe-only rendering**: Full 3D models require asset download pipeline (see roadmap)
2. **No texture support**: Material/texture application needs model loading
3. **UDP only**: WebSocket transport not yet implemented
4. **Entity types**: Limited to basic primitives (box, sphere, model placeholder)

## Roadmap

### Phase 1: Rendering ✅ (Current)
- [x] Wireframe entity visualization
- [x] Transform, color, dimension support
- [x] Entity type differentiation

### Phase 2: Assets (Planned)
- [ ] HTTP model downloader
- [ ] Local asset cache
- [ ] GLTF/GLB model loading
- [ ] Dynamic model replacement
- [ ] Texture application

### Phase 3: Interaction
- [ ] Input forwarding (pointer, hand tracking)
- [ ] Avatar representation
- [ ] Audio spatial rendering

### Phase 4: Advanced Features
- [ ] Script integration
- [ ] Zone/environment support
- [ ] Particle effects
- [ ] Lighting

## Troubleshooting

### "Failed to connect to StardustXR compositor"
- Ensure Stardust server is running
- Check `STARDUSTXR_SOCKET` environment variable
- Try: `ss -lx | grep stardust` to find the socket

### "Rust bridge present but start() failed"
- Rebuild the bridge: `cd bridge && cargo build --release`
- Check library exists: `ls -lh bridge/target/release/libstardust_bridge.so`
- Verify RPATH: `ldd build/stardust-overte-client`

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
