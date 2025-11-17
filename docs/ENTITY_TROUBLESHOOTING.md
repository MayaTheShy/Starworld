# Entity Troubleshooting Guide

## Overview

This guide helps diagnose issues with entity rendering and reception in Starworld.

## Debug Logging

Starworld provides comprehensive debug logging controlled by environment variables. Enable these to diagnose entity-related issues:

### Available Debug Flags

```bash
# Enable entity packet debugging (shows packet structure and content)
export STARWORLD_DEBUG_ENTITY_PACKETS=1

# Enable entity lifecycle tracking (shows creation/update/deletion)
export STARWORLD_DEBUG_ENTITY_LIFECYCLE=1

# Enable general network packet debugging
export STARWORLD_DEBUG_NETWORK=1
```

### Example Usage

```bash
# Enable all debugging for maximum visibility
export STARWORLD_DEBUG_ENTITY_PACKETS=1
export STARWORLD_DEBUG_ENTITY_LIFECYCLE=1
export STARWORLD_DEBUG_NETWORK=1

# Run with authentication
./build/starworld --auth --overte=127.0.0.1:40104

# Or run in simulation mode to test rendering
export STARWORLD_SIMULATE=1
./build/starworld
```

## Common Issues

### 1. Entities Not Appearing

**Symptoms:**
- Domain connection succeeds
- DomainList packet received
- No EntityData packets received
- No entities visible in VR

**Diagnostic Steps:**

1. **Enable entity debugging:**
   ```bash
   export STARWORLD_DEBUG_ENTITY_PACKETS=1
   export STARWORLD_DEBUG_ENTITY_LIFECYCLE=1
   ./build/starworld --overte=127.0.0.1:40104
   ```

2. **Check EntityQuery transmission:**
   Look for log lines like:
   ```
   [OverteClient] Sent EntityQuery to entity-server (192.168.1.100:40102, 35 bytes, seq=5)
   ```

3. **Verify EntityServer address:**
   The DomainList reply should contain an EntityServer entry:
   ```
   [OverteClient] Assignment client 0: type=0 (EntityServer)
   ```

4. **Check for EntityData packets:**
   Look for:
   ```
   [OverteClient] Received EntityData packet (XXX bytes)
   [OverteClient] Entity added: EntityName (id=12345)
   ```

**Common Causes:**

- **No EntityServer running:** Domain server may not have an entity server configured
- **Anonymous connection limitation:** Some domains restrict entity data for anonymous users
- **Network/firewall issues:** EntityServer may be on a different port/address that's blocked
- **HMAC verification issue:** Server rejecting sourced packets (see NETWORK_PROTOCOL_INVESTIGATION.md)

**Solutions:**

- Try authenticated connection: `./build/starworld --auth`
- Verify domain server has entity-server running
- Check firewall rules for UDP port (typically domain UDP port)
- Test with simulation mode to verify rendering works: `export STARWORLD_SIMULATE=1`

### 2. Entities Received But Not Rendered

**Symptoms:**
- EntityData packets received and logged
- Entity count increases
- No visual models appear in VR

**Diagnostic Steps:**

1. **Check entity counts:**
   Look for lifecycle logging:
   ```
   [OverteClient/Lifecycle] Total entities: 5, Update queue: 5
   ```

2. **Verify entity properties:**
   With `STARWORLD_DEBUG_ENTITY_LIFECYCLE=1`:
   ```
   [OverteClient] Entity added: Chair (id=12345)
     Type: 3
     Position: (1.5, 0.0, -2.0)
     Dimensions: (0.5, 0.5, 0.5)
     Model: https://example.com/models/chair.glb
   ```

3. **Check for zero dimensions:**
   Entities with zero dimensions are skipped:
   ```
   [bridge/reify] Skipping node 12345 (zero dimensions)
   ```

4. **Verify model loading:**
   Check bridge logs for model loading:
   ```
   [bridge/reify] Loading 3D model for node 12345 from URL: ...
   [bridge/reify] Using downloaded model: /home/user/.cache/starworld/models/abc123.glb
   ```

**Common Causes:**

- **Zero dimensions:** Entity has dimensions (0, 0, 0)
- **Missing primitive models:** Cache directory `~/.cache/starworld/primitives/` is empty
- **Model download failure:** HTTP download failed or URL is invalid
- **Stardust bridge not loaded:** Bridge library failed to load or initialize

**Solutions:**

- Generate primitive models:
  ```bash
  blender --background --python tools/blender_export_simple.py
  ```

- Check cache directories exist:
  ```bash
  ls -la ~/.cache/starworld/primitives/
  ls -la ~/.cache/starworld/models/
  ```

- Verify bridge is loaded:
  ```
  [StardustBridge] Rust bridge present: libstardust_bridge.so
  ```

### 3. Connection Drops After 11-18 Seconds

**Symptoms:**
- Initial connection succeeds
- DomainList received
- Connection killed after 11-18 seconds
- Error: "Node killed by domain server (silent node)"

**Cause:**
This is a known HMAC verification issue on the server side. See [docs/NETWORK_PROTOCOL_INVESTIGATION.md](NETWORK_PROTOCOL_INVESTIGATION.md) for detailed analysis.

**Workaround:**
Currently under investigation. The client implementation is correct; the issue is server-side configuration.

### 4. Entity Colors Not Applied

**Symptoms:**
- Entities appear with default/white color
- Entity color logged but not visible

**Cause:**
Color tinting is not yet implemented. The StardustXR asteroids Model element doesn't currently expose material manipulation APIs.

**Current State:**
- Entity colors are parsed from packets ✅
- Colors stored in entity structure ✅
- Colors propagated to Rust bridge ✅
- Colors logged in debug mode ✅
- Colors applied to model materials ❌ (TODO)

**Expected Logs:**
```
[bridge/reify] Node 12345 has color tint: RGBA(1.00, 0.00, 0.00, 1.00) - NOT YET APPLIED
```

### 5. Textures Not Applied

**Symptoms:**
- Entities show model geometry but no textures
- Texture URL logged but not visible

**Current State:**
- Texture URLs parsed from packets ✅
- Texture download system implemented ✅
- HTTP/HTTPS texture downloads cached ✅
- Textures passed to bridge ✅
- Textures applied to materials ❌ (Depends on StardustXR API)

**Expected Logs:**
```
[StardustBridge] Texture downloaded: https://example.com/texture.jpg -> /home/user/.cache/starworld/models/xyz789.jpg
[bridge/reify] Node 12345 has texture URL: /home/user/.cache/starworld/models/xyz789.jpg - NOT YET APPLIED
```

## Testing Entity Rendering

### Simulation Mode

Test entity rendering without connecting to a server:

```bash
export STARWORLD_SIMULATE=1
export STARWORLD_BRIDGE_PATH=/home/runner/work/Starworld/Starworld/bridge/target/release
./build/starworld
```

This creates three test entities:
- Red cube (Box type, 0.2m)
- Green sphere (Sphere type, 0.15m)  
- Blue suzanne (Model type, 0.25m)

### Unit Tests

Run the test harness to verify packet parsing:

```bash
cd build
./starworld-tests
```

Expected output:
```
[TEST] Protocol signature hex=eb1600e798dc5e03c755a968dc16b7fc
[TEST] Entity packet structure: 75 bytes
ALL TESTS PASS
```

## Log Analysis

### Successful Entity Reception

```
[OverteClient] Connected to domain server
[OverteClient] DomainList received
[OverteClient] Assignment client 0: type=0 (EntityServer) at 192.168.1.100:40102
[OverteClient] Sent EntityQuery to entity-server (192.168.1.100:40102, 35 bytes, seq=3)
[OverteClient] Received EntityData packet (523 bytes)
[OverteClient] Entity added: Chair (id=12345)
[OverteClient/Lifecycle] Total entities: 1, Update queue: 1
[bridge/reify] Reifying 1 nodes
[bridge/reify] Loading 3D model for node 12345 from URL: https://example.com/chair.glb
[StardustBridge] Model downloaded: https://example.com/chair.glb -> /home/user/.cache/starworld/models/abc123.glb
```

### Failed Entity Reception

```
[OverteClient] Connected to domain server
[OverteClient] DomainList received
[OverteClient] No EntityServer found in assignment clients
[OverteClient] Sent EntityQuery to domain-server (192.168.1.100:40104, 35 bytes, seq=3)
[OverteClient] (No EntityData packets received)
[OverteClient/Lifecycle] Total entities: 0, Update queue: 0
```

## Environment Variables Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `STARWORLD_SIMULATE` | `0` | Enable simulation mode (1=on) |
| `STARWORLD_DEBUG_ENTITY_PACKETS` | `0` | Log entity packet contents |
| `STARWORLD_DEBUG_ENTITY_LIFECYCLE` | `0` | Log entity creation/updates |
| `STARWORLD_DEBUG_NETWORK` | `0` | Log all network packets |
| `STARWORLD_BRIDGE_PATH` | `./bridge/target/debug` | Path to Rust bridge library |
| `OVERTE_UDP_PORT` | from URL | Override UDP domain server port |

## Getting Help

If you encounter issues not covered here:

1. Enable all debug logging
2. Capture full output to a file:
   ```bash
   export STARWORLD_DEBUG_ENTITY_PACKETS=1
   export STARWORLD_DEBUG_ENTITY_LIFECYCLE=1
   ./build/starworld --overte=127.0.0.1:40104 2>&1 | tee debug.log
   ```

3. Check existing documentation:
   - [NETWORK_PROTOCOL_INVESTIGATION.md](NETWORK_PROTOCOL_INVESTIGATION.md) - Connection issues
   - [ENTITY_RENDERING_ENHANCEMENTS.md](ENTITY_RENDERING_ENHANCEMENTS.md) - Rendering details
   - [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) - Build and development

4. File an issue with:
   - Debug log output
   - Domain server address/version
   - Expected vs actual behavior
   - Steps to reproduce
