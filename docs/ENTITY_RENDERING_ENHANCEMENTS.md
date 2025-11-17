# Entity Rendering Implementation for Starworld

## Overview

This document describes the entity rendering system in Starworld, which loads and displays Overte entities as 3D GLTF/GLB models in the StardustXR compositor.

## Current Implementation

### 1. Entity Data Structure (`OverteClient.hpp`)

**`EntityType` enum:**
```cpp
enum class EntityType {
    Unknown, Box, Sphere, Model, Shape, Light, Text, 

    ## Current Implementation

    All core entity rendering features are implemented:

    - 3D model rendering (GLTF/GLB) for Box, Sphere, Model types
    - HTTP/HTTPS model and texture download and caching (SHA256-based)
    - Primitive model generation (cube, sphere, suzanne) with Blender
    - Transform, dimension, and color/texture data parsing and propagation
    - Color and texture download infrastructure is complete, but visual application is pending StardustXR API support
    - See [`docs/ENTITY_TROUBLESHOOTING.md`](ENTITY_TROUBLESHOOTING.md) for debug flags and troubleshooting

    ## Testing

    See [`README.md`](../README.md) and [`docs/IMPLEMENTATION_COMPLETE.md`](IMPLEMENTATION_COMPLETE.md) for up-to-date build and test instructions.

    ## Limitations

    - Color tinting and texture application are not yet visually applied (pending StardustXR API extension)
    - Only Box, Sphere, Model entity types are supported
    - atp:// protocol is not yet supported
    - See [`docs/IMPLEMENTATION_COMPLETE.md`](IMPLEMENTATION_COMPLETE.md) for full status

    ## References

    - Overte protocol: `third_party/overte-src/libraries/networking/`
    - Stardust API: https://github.com/StardustXR/core
    - See `docs/` for protocol, troubleshooting, and implementation details
```

This creates three demo entities:
- **CubeA** - Red cube model (20cm)
- **SphereB** - Green sphere model (15cm)
- **ModelC** - Blue Suzanne model (25cm)

### Live Overte Connection

To connect to a real Overte server:

```bash
# Optional: Set credentials
export OVERTE_USERNAME=your_username

# Connect to server
./build/starworld ws://domain.example.com:40102
```

The client will:
1. Perform domain handshake
2. Discover entity server via DomainList packets
3. Send EntityQuery to request all entities
4. Parse and render entities with full visual properties

## Architecture

```
┌─────────────────┐
│ Overte Server   │
│ (Entity Server) │
└────────┬────────┘
         │ UDP Packets (EntityAdd, EntityEdit)
         │ Contains: type, position, rotation, dimensions,
         │          color, modelUrl, textureUrl
         ▼
┌─────────────────────┐
│  OverteClient.cpp   │
│  - parseEntityPacket│
│  - OverteEntity     │
└────────┬────────────┘
         │ consumeUpdatedEntities()
         ▼
┌─────────────────────┐
│   SceneSync.cpp     │
│  - Maps Overte IDs  │
│    to Stardust IDs  │
└────────┬────────────┘
         │ createNode(), setNodeColor(), 
         │ setNodeModel(), etc.
         ▼
┌─────────────────────┐
│ StardustBridge.cpp  │
│  - C++ wrapper      │
│  - dlopen bridge.so │
└────────┬────────────┘
         │ C-ABI calls: sdxr_set_node_*
         ▼
┌─────────────────────┐
│ bridge/lib.rs       │
│  - Command queue    │
│  - Shared state     │
└────────┬────────────┘
         │ BridgeState::reify()
         ▼
┌─────────────────────┐
│  Stardust Server    │
│  - Model rendering  │
│  - GLTF/GLB loading │
└─────────────────────┘
```

## Implemented Features ✅

1. **3D Model Rendering** - Loads GLTF/GLB models using Stardust Model element
2. **Entity Type Support** - Box (cube), Sphere, Model (Suzanne placeholder)
3. **Transform Support** - Position, rotation, scale from dimensions
4. **HTTP Downloads** - ModelCache with SHA256 caching, async libcurl
5. **Primitive Generation** - Blender export script for test models
6. **Local Caching** - Two-tier cache (primitives + downloaded models)

## Future Enhancements

### Short Term
1. **Material color application** - Apply entity.color to model materials
2. **Texture support** - Load and apply entity.textureUrl to models
3. **More entity types** - Add support for Light, Text, PolyLine, etc.
4. **ATP protocol** - Support atp:// URLs for Overte asset server

### Medium Term
1. **Material parameters** - PBR materials with metallic/roughness
2. **Animation** - Skeletal animation for rigged models
3. **Entity updates** - Real-time property changes
4. **Entity deletion** - Remove entities from scene

### Long Term
1. **Physics sync** - Real-time physics state synchronization
2. **Script integration** - Execute Overte entity scripts in Stardust context
3. **Avatar rendering** - Full avatar mesh and animation support
4. **Audio spatialization** - 3D positional audio from Overte

## Build Instructions

```bash
# Build Rust bridge
cd bridge
cargo build
cd ..

# Build C++ client
cd build
cmake ..
make
cd ..

# Run
./build/starworld
```

## Dependencies

- **C++17** - Modern C++ features
- **GLM** - Math library for vectors/matrices
- **OpenSSL** - MD5 hashing for protocol signatures
- **Rust nightly** - For Stardust bridge
- **stardust-xr-asteroids** - Declarative UI framework
- **stardust-xr-fusion** - Low-level client API
- **tokio** - Async runtime

## References

- Overte protocol: `third_party/overte-src/libraries/networking/`
- Entity types: Based on Overte `EntityTypes.h`
- Packet formats: Overte NLPacket specification
- Stardust API: https://github.com/StardustXR/core
