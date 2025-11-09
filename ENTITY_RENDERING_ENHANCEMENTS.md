# Entity Rendering Implementation for Starworld

## Overview

This document describes the entity rendering system in Starworld, which loads and displays Overte entities as 3D GLTF/GLB models in the StardustXR compositor.

## Current Implementation

### 1. Entity Data Structure (`OverteClient.hpp`)

**`EntityType` enum:**
```cpp
enum class EntityType {
    Unknown, Box, Sphere, Model, Shape, Light, Text, 
    Zone, Web, ParticleEffect, Line, PolyLine, Grid, Gizmo, Material
};
```

**`OverteEntity` structure:**
```cpp
struct OverteEntity {
    std::uint64_t id{0};
    std::string name;
    glm::mat4 transform{1.0f};
    
    // Visual properties
    EntityType type{EntityType::Box};
    std::string modelUrl;      // For Model type entities
    std::string textureUrl;    // Texture/material URL
    glm::vec3 color{1.0f, 1.0f, 1.0f};  // RGB color (0-1 range)
    glm::vec3 dimensions{0.1f, 0.1f, 0.1f};  // Size/scale in meters
    float alpha{1.0f};         // Transparency (0-1)
};
```

### 2. Entity Packet Parser (`OverteClient.cpp`)

The `parseEntityPacket()` function extracts:
- Entity type classification
- Model URLs (for 3D models)
- Texture URLs
- RGB color values
- Dimensions/scale
- Alpha transparency

Simulation mode creates diverse entity types:
- Red cube (Box type)
- Green sphere (Sphere type)  
- Blue suzanne model (Model type)

### 3. Rust Bridge Node Structure (`bridge/src/lib.rs`)

**`Node` structure with entity data:**
```rust
struct Node {
    id: u64,
    name: String,
    transform: Mat4,
    entity_type: u8,       // 0=Unknown, 1=Box, 2=Sphere, 3=Model
    model_url: String,
    texture_url: String,
    color: [f32; 4],       // RGBA
    dimensions: [f32; 3],  // xyz dimensions
}
```

**C-ABI export functions:**
- `sdxr_set_node_model(id, model_url)` - Set model URL
- `sdxr_set_node_texture(id, texture_url)` - Set texture URL
- `sdxr_set_node_color(id, r, g, b, a)` - Set RGBA color
- `sdxr_set_node_dimensions(id, x, y, z)` - Set dimensions
- `sdxr_set_node_entity_type(id, type)` - Set entity type

### 4. 3D Model Rendering (`bridge/src/lib.rs` - `reify()`)

**Current implementation uses GLTF/GLB model loading:**

The rendering system loads pre-generated primitive models based on entity type:

```rust
fn get_model_path(entity_type: u8) -> Option<PathBuf> {
    let cache_dir = dirs::cache_dir()?.join("starworld/primitives");
    let filename = match entity_type {
        1 => "cube.glb",      // Box
        2 => "sphere.glb",    // Sphere  
        3 => "model.glb",     // Model (Suzanne placeholder)
        _ => return None,
    };
    // ...
}
```

**Model Loading Process:**
1. Determine entity type (Box, Sphere, Model)
2. Look up corresponding GLTF/GLB file in cache
3. Load model using `Model::direct(PathBuf)`
4. Apply transform (position, rotation, scale from dimensions)
5. Render in StardustXR scene

**Features:**
- Respects entity dimensions for sizing
- Applies proper transforms (position, rotation, scale)
- Loads models asynchronously
- Provides error logging for missing models
- Uses cached primitives for Box, Sphere, Model types
- Model entity type loads Suzanne (Blender monkey head) as placeholder

**Primitive Model Generation:**

Models are generated using `tools/blender_export_simple.py`:
- Creates cube.glb, sphere.glb, model.glb (Suzanne)
- Exports to `~/.cache/starworld/primitives/`
- Run: `blender --background --python tools/blender_export_simple.py`

### 5. HTTP Asset Downloading (`ModelCache.cpp/.hpp`)

**ModelCache singleton** handles HTTP/HTTPS model downloads:

```cpp
ModelCache::instance().requestModel(
    "https://example.com/models/chair.glb",
    [](const std::string& url, bool success, const std::string& localPath) {
        if (success) {
            // Pass localPath to bridge for rendering
        }
    }
);
```

**Features:**
- SHA256-based filename hashing for cache
- Async downloads with libcurl
- Progress callbacks
- Caches to `~/.cache/starworld/models/`
- Thread-safe resource tracking

### 6. StardustBridge C++ Interface (`StardustBridge.hpp/.cpp`)

**Bridge methods:**
```cpp
bool setNodeModel(NodeId id, const std::string& modelUrl);
bool setNodeTexture(NodeId id, const std::string& textureUrl);
bool setNodeColor(NodeId id, const glm::vec3& color, float alpha = 1.0f);
bool setNodeDimensions(NodeId id, const glm::vec3& dimensions);
bool setNodeEntityType(NodeId id, uint8_t entityType);
```

**HTTP model handling:**
- Detects http:// and https:// URLs
- Requests download via ModelCache
- Passes local cached path to Rust bridge
- Fallback to direct URLs for file://, atp://, etc.

### 7. SceneSync Integration (`SceneSync.cpp`)

**Entity synchronization:**
- Propagates entity type on creation/update
- Sets color and alpha properties
- Configures dimensions
- Handles model and texture URLs
- Updates visual properties when entities change

## Testing

### Simulation Mode

Run with simulation mode to see example entities:

```bash
export STARWORLD_SIMULATE=1
./build/starworld
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
