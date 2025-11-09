# Quick Test Guide - Overte Entity Rendering

## Quick Start Test (Simulation Mode)

This tests the rendering pipeline without needing an Overte server:

```bash
# 1. Build everything
cd /home/mayatheshy/stardust/Starworld
./scripts/build_and_test.sh

# 2. Generate primitive models (if not already done)
blender --background --python tools/blender_export_simple.py

# 3. Start StardustXR server (in separate terminal)
stardust-xr-server

# 4. Run in simulation mode
export STARWORLD_SIMULATE=1
export STARWORLD_BRIDGE_PATH=./bridge/target/release
./build/starworld
```

**What You Should See**:
- Console output showing 3 entities created
- In XR: Red cube, green sphere, and blue Suzanne monkey head
- Models positioned at different heights in front of you

## Test with Local Overte Domain

```bash
# 1. Start local Overte domain server
# (See Overte documentation for domain server setup)

# 2. Connect Starworld client
./build/starworld --overte=127.0.0.1:40104
```

**What You Should See**:
- Domain handshake messages
- Entity query sent
- Entities appearing as they're received
- Models loading from primitives or downloaded URLs

## Test with HTTP Model URLs

To test HTTP model downloading:

1. **Add entities with model URLs in Overte**:
   - Use Overte's Create app to add Model entities
   - Set modelURL to an HTTP/HTTPS GLTF/GLB file
   - Example: `https://example.com/models/chair.glb`

2. **Run Starworld**:
   ```bash
   ./build/starworld --overte=127.0.0.1:40104
   ```

3. **Check console for download progress**:
   ```
   [downloader] Downloading model: https://example.com/chair.glb
   [downloader] Downloaded successfully: /home/user/.cache/starworld/models/abc123.glb
   [bridge/reify] Loading 3D model for node 1 from URL: https://example.com/chair.glb
   ```

4. **Verify cached files**:
   ```bash
   ls -lh ~/.cache/starworld/models/
   ```

## Debug Checklist

### If nothing appears in XR:

1. **Check StardustXR server is running**:
   ```bash
   ps aux | grep stardust
   ```

2. **Check bridge loaded successfully**:
   ```
   # In Starworld output, look for:
   [StardustBridge] Loading Rust bridge from: ...
   [StardustBridge] Rust bridge loaded and started successfully
   ```

3. **Check primitives exist**:
   ```bash
   ls ~/.cache/starworld/primitives/
   # Should show: cube.glb, sphere.glb, model.glb
   ```

4. **Enable verbose logging**:
   ```bash
   export RUST_LOG=debug
   ./build/starworld
   ```

### If models don't download:

1. **Check internet connectivity**:
   ```bash
   curl -I https://example.com/model.glb
   ```

2. **Check cache directory permissions**:
   ```bash
   ls -ld ~/.cache/starworld/models/
   ```

3. **Look for download errors in console**:
   ```
   [downloader] Failed to download https://...: ...
   ```

### If Overte connection fails:

1. **Verify domain server is running**:
   ```bash
   ps aux | grep domain-server
   ```

2. **Check UDP port is correct**:
   ```bash
   # Overte domain server typically uses 40104 for UDP
   sudo ss -ulnp | grep 40104
   ```

3. **Check for connection messages**:
   ```
   [OverteClient] Attempting to connect to domain: 127.0.0.1:40104
   [OverteClient] Domain handshake sent
   [OverteClient] Received DomainList packet
   ```

## Expected Console Output (Simulation Mode)

```
[StardustBridge] Loading Rust bridge from: ./bridge/target/release/libstardust_bridge.so
[StardustBridge] Rust bridge loaded successfully
[StardustBridge] Rust bridge started: sdxr_start() returned 0
[StardustBridge] Rust bridge loaded and started successfully
[bridge] Connecting to Stardust server...
[OverteClient] Simulation mode enabled - creating test entities
[bridge] create node id=1 name=CubeA (state nodes=1)
[bridge] set entity type for node id=1: 1
[bridge] set color for node id=1: [1.0, 0.0, 0.0, 1.0]
[bridge] set dimensions for node id=1: [0.2, 0.2, 0.2]
[bridge] create node id=2 name=SphereB (state nodes=2)
[bridge] set entity type for node id=2: 2
[bridge] set color for node id=2: [0.0, 1.0, 0.0, 1.0]
[bridge] set dimensions for node id=2: [0.15, 0.15, 0.15]
[bridge] create node id=3 name=ModelC (state nodes=3)
[bridge] set entity type for node id=3: 3
[bridge] set color for node id=3: [0.0, 0.0, 1.0, 1.0]
[bridge] set dimensions for node id=3: [0.25, 0.25, 0.25]
[bridge/reify] Reifying 3 nodes
[bridge/reify] Loading cube for node 1 primitive from /home/user/.cache/starworld/primitives/cube.glb
[bridge/reify] Node 1 has color tint: RGBA(1.00, 0.00, 0.00, 1.00) - NOT YET APPLIED
[bridge/reify] Loading sphere for node 2 primitive from /home/user/.cache/starworld/primitives/sphere.glb
[bridge/reify] Node 2 has color tint: RGBA(0.00, 1.00, 0.00, 1.00) - NOT YET APPLIED
[bridge/reify] Loading 3D model for node 3 primitive from /home/user/.cache/starworld/primitives/model.glb
[bridge/reify] Node 3 has color tint: RGBA(0.00, 0.00, 1.00, 1.00) - NOT YET APPLIED
```

## Performance Testing

### Test with Many Entities

Create a domain with 100+ entities and monitor:

```bash
# Run with timing
time ./build/starworld --overte=127.0.0.1:40104

# Monitor memory usage
watch -n 1 'ps aux | grep starworld'

# Check frame rate (in Stardust server logs)
journalctl --user -u stardust -f
```

### Test Download Performance

Test with large models (10MB+):

```bash
# Clear cache first
rm -rf ~/.cache/starworld/models/*

# Connect and time first load
time ./build/starworld --overte=127.0.0.1:40104

# Second run should be instant (cached)
time ./build/starworld --overte=127.0.0.1:40104
```

## Common Test Scenarios

### 1. Mixed Entity Types
- Create Box, Sphere, and Model entities in Overte
- Verify each renders with correct primitive
- Check dimensions are applied correctly

### 2. HTTP Model Downloads
- Add Model entity with HTTP URL
- Watch console for download progress
- Verify model appears after download
- Reconnect - should load from cache instantly

### 3. Entity Updates
- Move/rotate entities in Overte
- Verify updates appear in StardustXR
- Check transform synchronization

### 4. Entity Deletion
- Delete entities in Overte
- Verify they disappear from StardustXR
- Check console for removal messages

## Success Criteria

✅ Simulation mode shows 3 colored primitives  
✅ Overte connection establishes successfully  
✅ Entities from Overte appear in StardustXR  
✅ HTTP models download and render  
✅ Downloaded models are cached  
✅ Entity transforms match Overte positions  
✅ Dimensions scale models correctly  
✅ Entity deletions are reflected  

## Troubleshooting Quick Reference

| Problem | Solution |
|---------|----------|
| Nothing appears | Check StardustXR server running, check primitives exist |
| Bridge fails to load | Rebuild bridge: `cd bridge && cargo build --release` |
| Downloads fail | Check internet, check URL accessibility, check cache permissions |
| Connection refused | Verify domain server running, check port number (40104) |
| Models too small/large | Check entity dimensions in Overte, verify scale application |
| Old models shown | Clear cache: `rm -rf ~/.cache/starworld/models/*` |

---

**Last Updated**: November 9, 2025  
**Tested Version**: Starworld dev branch  
**Platform**: Linux (Arch-based)
