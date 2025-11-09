# Starworld Developer Quick Reference

## Build Commands

```bash
# Full clean build
./build_and_test.sh

# Build Rust bridge only
cd bridge && cargo build --release

# Build C++ client only
cmake --build build --target starworld -j$(nproc)

# Rebuild bridge (debug mode)
cd bridge && cargo build

# Clean everything
rm -rf build bridge/target
```

## Run Commands

```bash
# Simulation mode (no Overte server needed)
STARWORLD_SIMULATE=1 ./build/starworld

# Connect to local domain
./build/starworld --overte=127.0.0.1:40104

# Connect to remote domain
./build/starworld --overte=domain.example.com:40104

# Domain discovery
./build/starworld --discover

# With verbose logging
RUST_LOG=debug ./build/starworld
```

## Test Commands

```bash
# Run all tests
./ci-test.sh

# Run C++ tests only
./build/starworld-tests

# Run Rust tests only
cd bridge && cargo test

# Check code style
cd bridge && cargo fmt --check
cd bridge && cargo clippy
```

## Debugging

### Check Domain Server Status
```bash
# Is domain server running?
ps aux | grep domain-server

# What ports is it listening on?
sudo ss -ulnp | grep domain-server

# Check for assignment clients
ps aux | grep assignment-client
```

### Check StardustXR Connection
```bash
# Is Stardust server running?
ps aux | grep stardust

# Find Stardust socket
ss -lx | grep stardust

# Check Stardust logs
journalctl --user -u stardust -f
```

### Debug Overte Connection
```bash
# Test network connectivity
ping 127.0.0.1
nc -zvu 127.0.0.1 40104  # If nc available

# Capture Overte packets (requires root)
sudo tcpdump -i lo -n udp port 40104 -X

# Run with connection logging
./build/starworld --overte=127.0.0.1:40104 2>&1 | tee connection.log
```

### Check Model Cache
```bash
# List cached primitive models
ls -lh ~/.cache/starworld/primitives/

# List downloaded models
ls -lh ~/.cache/starworld/models/

# Check export log
cat ~/.cache/starworld/primitives/export_log.txt

# Regenerate primitives
python3 tools/blender_export_simple.py
```

## Common Issues

### "Failed to connect to StardustXR compositor"
```bash
# Start StardustXR server first
stardust-xr-server &

# Or check if it's running
ps aux | grep stardust
```

### "Rust bridge present but start() failed"
```bash
# Rebuild bridge
cd bridge && cargo build --release

# Check library exists
ls -lh bridge/target/release/libstardust_bridge.so

# Check for missing dependencies
ldd build/starworld
```

### "Retrying domain handshake..."
```bash
# Domain server not responding - check if running
ps aux | grep domain-server

# Try local server first
./build/starworld --overte=127.0.0.1:40104

# Check firewall for remote domains
sudo ufw status
```

### Nothing renders in XR
```bash
# Check if entities exist
RUST_LOG=debug ./build/starworld 2>&1 | grep -i entity

# Use simulation mode
STARWORLD_SIMULATE=1 ./build/starworld

# Check bridge logs
./build/starworld 2>&1 | grep '\[bridge'
```

## File Locations

### Source Code
- `src/main.cpp` - Entry point, argument parsing
- `src/OverteClient.cpp` - Overte protocol implementation
- `src/StardustBridge.cpp` - C++/Rust bridge interface
- `src/SceneSync.cpp` - Entity synchronization
- `src/NLPacketCodec.cpp` - Packet encoding/decoding
- `src/QDataStream.cpp` - Qt serialization
- `bridge/src/lib.rs` - Rust StardustXR client

### Configuration
- `CMakeLists.txt` - C++ build configuration
- `bridge/Cargo.toml` - Rust build configuration
- `.gitea/workflows/` - CI/CD pipelines

### Documentation
- `README.md` - Main documentation
- `OVERTE_AUTH.md` - OAuth implementation guide
- `OVERTE_ASSIGNMENT_CLIENT_TASK.md` - Protocol details
- `CHANGELOG.md` - Version history
- `ENTITY_RENDERING_ENHANCEMENTS.md` - Rendering implementation
- `MODELCACHE_IMPLEMENTATION.md` - Asset pipeline

### Tests
- `tests/TestHarness.cpp` - C++ unit tests
- `bridge/src/lib.rs` - Rust unit tests (inline)

## Key Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `STARWORLD_SIMULATE` | Enable simulation mode | `1` |
| `STARWORLD_BRIDGE_PATH` | Override bridge location | `./bridge/target/release` |
| `OVERTE_UDP_PORT` | Override UDP port | `40104` |
| `OVERTE_DISCOVER` | Enable domain discovery | `1` |
| `RUST_LOG` | Rust logging level | `debug` |
| `STARDUSTXR_SOCKET` | Override Stardust socket | `/run/user/1000/stardust-socket` |

## Protocol Quick Reference

### Port Numbers
- **40102**: HTTP/WebSocket (domain server web interface)
- **40104**: UDP (domain server main communication)
- **Dynamic**: Assignment clients (EntityServer, AudioMixer, etc.)

### Packet Types
- `0x1F` (31): DomainConnectRequest
- `0x02` (2): DomainList reply
- `0x03` (3): DomainListRequest
- `0x05` (5): Ping
- `0x06` (6): PingReply
- `0x10` (16): EntityQuery

### Connection Flow
```
1. Client → Domain: DomainConnectRequest (UDP 40104)
2. Domain → Client: DomainList (session UUID, local ID, assignment clients)
3. Client → Domain: Periodic Ping (keep-alive)
4. Client → EntityServer: EntityQuery (or domain as fallback)
5. EntityServer → Client: EntityData (not yet implemented)
```

## Architecture Diagram

```
┌─────────────────┐
│  StardustXR     │
│  Compositor     │
└────────┬────────┘
         │ Unix Socket
┌────────▼────────┐
│  Rust Bridge    │
│  (stardust-xr-  │
│   fusion)       │
└────────┬────────┘
         │ C ABI (dlopen)
┌────────▼────────┐
│  StardustBridge │
│  (C++)          │
└────────┬────────┘
         │
┌────────▼────────┐
│  OverteClient   │
│  (C++)          │
└────────┬────────┘
         │ UDP (NLPacket)
┌────────▼────────┐
│  Overte Domain  │
│  Server         │
└─────────────────┘
```

## Useful Commands

```bash
# Find all TODO comments
grep -rn "TODO\|FIXME" src/ bridge/src/

# Count lines of code
cloc src/ bridge/src/

# Generate compile_commands.json
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON build

# Format Rust code
cd bridge && cargo fmt

# Update dependencies
cd bridge && cargo update

# Check for security vulnerabilities
cd bridge && cargo audit

# Build documentation
cd bridge && cargo doc --open
```

## Performance Profiling

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo build
make -j$(nproc)

# Run with perf
perf record -g ./build/starworld
perf report

# Memory profiling with valgrind
valgrind --leak-check=full ./build/starworld

# Rust flamegraph (requires cargo-flamegraph)
cd bridge && cargo flamegraph
```

## Git Workflow

```bash
# Start new feature
git checkout -b feature/my-feature

# Make changes, commit
git add src/OverteClient.cpp
git commit -m "Add entity color support"

# Push and create PR
git push origin feature/my-feature
# Then create PR in Gitea web UI

# Update from main
git fetch origin
git rebase origin/main
```

## Resources

### StardustXR
- **Website**: https://stardustxr.org
- **GitHub**: https://github.com/StardustXR
- **Core (Fusion)**: https://github.com/StardustXR/core - Client library for StardustXR
- **Server**: https://github.com/StardustXR/server - XR compositor/display server
- **Asteroids**: https://github.com/StardustXR/asteroids - UI element library
- **Flatland**: https://github.com/StardustXR/flatland - 2D panel server
- **Magnetar**: https://github.com/StardustXR/magnetar - Input fusion server
- **Documentation**: https://stardustxr.org/docs
- **Matrix Chat**: https://matrix.to/#/#stardustxr:matrix.org

### Overte
- **Website**: https://overte.org - Official Overte homepage
- **GitHub**: https://github.com/overte-org/overte - Main source repository
- **User Docs**: https://docs.overte.org - End-user documentation
- **Developer Docs**: https://docs.overte.org/developer - Developer guides
- **API Reference**: https://apidocs.overte.org - API documentation
- **Discord**: https://discord.gg/overte - Community chat
- **Metaverse**: https://mv.overte.org - Main public metaverse server
- **Forums**: https://forums.overte.org - Community discussions

### Overte Protocol Documentation
- **Networking Library**: https://github.com/overte-org/overte/tree/master/libraries/networking
- **NLPacket**: `libraries/networking/src/NLPacket.h` - Packet format
- **NodeList**: `libraries/networking/src/NodeList.cpp` - Domain connection
- **PacketHeaders**: `libraries/networking/src/PacketHeaders.h` - Packet types
- **EntityServer**: `assignment-client/src/entities/EntityServer.cpp` - Entity serving
- **DomainServer**: `domain-server/src/DomainServer.cpp` - Domain coordination

### Development Tools & Libraries
- **CMake**: https://cmake.org - Build system
- **Rust**: https://www.rust-lang.org - Rust language
- **Cargo**: https://doc.rust-lang.org/cargo/ - Rust package manager
- **GLM**: https://github.com/g-truc/glm - OpenGL Mathematics
- **libcurl**: https://curl.se/libcurl/ - HTTP client library
- **GLTF/GLB**: https://www.khronos.org/gltf/ - 3D model format
- **Blender**: https://www.blender.org - 3D creation suite

### Technical Specifications
- **Qt QDataStream**: https://doc.qt.io/qt-5/qdatastream.html - Overte serialization
- **OAuth 2.0**: https://www.rfc-editor.org/rfc/rfc6749.html - Authentication protocol
- **WebSocket**: https://www.rfc-editor.org/rfc/rfc6455.html - (Future: WebSocket transport)
- **UDP Protocol**: https://www.rfc-editor.org/rfc/rfc768.html - Current transport

### Community & Support
- **Starworld Issues**: https://git.spatulaa.com/MayaTheShy/Starworld/issues
- **StardustXR Matrix**: https://matrix.to/#/#stardustxr:matrix.org
- **Overte Discord**: https://discord.gg/overte
- **Overte Forums**: https://forums.overte.org
