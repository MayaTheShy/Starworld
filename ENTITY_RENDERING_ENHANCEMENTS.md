# Entity Rendering Enhancements for Starworld

## Overview

This document describes the enhancements made to starworld to support rendering complex Overte entities with full visual properties including models, textures, colors, and different geometric primitives.

## Changes Made

### 1. Enhanced Entity Data Structure (`OverteClient.hpp`)

**Added `EntityType` enum:**
```cpp
enum class EntityType {
    Unknown, Box, Sphere, Model, Shape, Light, Text, 
    Zone, Web, ParticleEffect, Line, PolyLine, Grid, Gizmo, Material
};
```

**Extended `OverteEntity` structure:**
```cpp
struct OverteEntity {
    std::uint64_t id{0};
    std::string name;
    glm::mat4 transform{1.0f};
    
    // NEW: Visual properties
    EntityType type{EntityType::Box};
    std::string modelUrl;      // For Model type entities
    std::string textureUrl;    // Texture/material URL
    glm::vec3 color{1.0f, 1.0f, 1.0f};  // RGB color (0-1 range)
    glm::vec3 dimensions{0.1f, 0.1f, 0.1f};  // Size/scale in meters
    float alpha{1.0f};         // Transparency (0-1)
};
```

### 2. Enhanced Entity Packet Parser (`OverteClient.cpp`)

**Updated `parseEntityPacket()` to extract:**
- Entity type classification
- Model URLs (for 3D models)
- Texture URLs
- RGB color values
- Dimensions/scale
- Alpha transparency

**Enhanced simulation mode** with diverse entity types:
- Red cube (Box type)
- Green sphere (Sphere type)  
- Blue model placeholder (Model type)

### 3. Rust Bridge Visual Properties (`bridge/src/lib.rs`)

**Extended `Node` structure:**
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

**Added new command types:**
- `SetModel` - Set 3D model URL
- `SetTexture` - Set texture/material URL
- `SetColor` - Set RGBA color
- `SetDimensions` - Set xyz dimensions
- `SetEntityType` - Set geometric primitive type

**New C-ABI export functions:**
- `sdxr_set_node_model(id, model_url)`
- `sdxr_set_node_texture(id, texture_url)`
- `sdxr_set_node_color(id, r, g, b, a)`
- `sdxr_set_node_dimensions(id, x, y, z)`
- `sdxr_set_node_entity_type(id, type)`

### 4. Enhanced Rendering (`bridge/src/lib.rs` - `reify()`)

**Implemented type-specific wireframe rendering:**

The current implementation uses wireframe visualizations for all entity types, providing a lightweight and performant rendering solution that works with the Stardust Lines API:

- **Box entities (type 1):** Colored wireframe cubes (12 edges) using entity color and dimensions
- **Sphere entities (type 2):** Three orthogonal wireframe circles (XY, XZ, YZ planes) forming a sphere visualization
- **Model entities (type 3):** Distinctive octahedron wireframe (12 edges) as a placeholder for 3D models
- **Unknown types:** Gray wireframe cube as fallback

**Features:**
- Respects entity dimensions for accurate sizing
- Uses entity-specific RGB colors with alpha transparency
- Applies proper transforms (position, rotation, scale)
- Maintains visual distinction between entity types
- Lightweight rendering using the Stardust Lines element

**Note on 3D Model Support:**
Full 3D model rendering with textures requires:
1. Downloading Overte model assets (typically GLTF/GLB format)
2. Caching them locally
3. Using `Model::direct(PathBuf)` with local file paths
4. The current implementation uses wireframe placeholders as Overte model URLs are HTTP-based and require asset pipeline integration

Future enhancement: Implement a background asset downloader that fetches Overte models and textures, caches them locally, and dynamically updates the scene graph to replace wireframe placeholders with actual 3D models.

### 5. StardustBridge C++ Interface (`StardustBridge.hpp/.cpp`)

**Added methods:**
```cpp
bool setNodeModel(NodeId id, const std::string& modelUrl);
bool setNodeTexture(NodeId id, const std::string& textureUrl);
bool setNodeColor(NodeId id, const glm::vec3& color, float alpha = 1.0f);
bool setNodeDimensions(NodeId id, const glm::vec3& dimensions);
bool setNodeEntityType(NodeId id, uint8_t entityType);
```

**Updated dynamic library loader** to resolve new function pointers from Rust bridge.

### 6. SceneSync Integration (`SceneSync.cpp`)

**Enhanced entity synchronization:**
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
./build/stardust-overte-client
```

This creates three demo entities:
- **CubeA** - Red wireframe cube (20cm)
- **SphereB** - Green wireframe sphere (15cm)
- **ModelC** - Blue octahedron representing a model entity (25cm)

### Live Overte Connection

To connect to a real Overte server:

```bash
# Optional: Set credentials
export OVERTE_USERNAME=your_username

# Connect to server
./build/stardust-overte-client ws://domain.example.com:40102
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
│  - Lines rendering  │
│  - Type-specific    │
│    visualization    │
└─────────────────────┘
```

## Future Enhancements

### Short Term
1. **Actual model loading** - Replace octahedron with loaded .glb/.fbx models using Stardust Model nodes
2. **Texture application** - Apply textures to rendered entities
3. **More entity types** - Add support for Light, Text, PolyLine, etc.
4. **Performance optimization** - Batch updates, reduce command overhead

### Medium Term
1. **Full mesh rendering** - Move beyond wireframes to solid shaded meshes
2. **Material support** - PBR materials with metallic/roughness
3. **Animation** - Skeletal animation for rigged models
4. **LOD system** - Level-of-detail based on distance

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
./build/stardust-overte-client
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
