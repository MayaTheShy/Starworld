# Future Enhancements - Overte to Stardust Entity Rendering

## Overview

This document outlines the features that are **not yet implemented** but would be valuable additions to complete the Overte-to-Stardust integration. All core functionality for basic 3D model rendering is working, but these enhancements would improve visual fidelity and feature parity with native Overte.

## Priority 1: Visual Fidelity

### 1.1 Color Tinting for Models

**Status**: 🟡 Data captured, not applied  
**Difficulty**: Medium  
**Requires**: Extension to asteroids Model API or server-side material manipulation

**Current State**:
- Entity color values (RGB + alpha) are parsed from Overte packets ✅
- Color data is stored in the Rust bridge state ✅
- Color is logged during rendering ✅
- Color is NOT applied to model materials ❌

**What's Needed**:
1. **Option A: Asteroids API Extension**
   ```rust
   Model::direct(&path)
       .color_tint([r, g, b, a])  // New API needed
       .build()
   ```

2. **Option B: Material Modification in reify()**
   - Access model's materials after loading
   - Multiply base color by tint color
   - Update material in Stardust server
   - Requires deeper integration with Bevy's material system

**Implementation Sketch**:
```rust
// In reify() function
match Model::direct(&model_path) {
    Ok(model) => {
        // Hypothetical API:
        let tinted_model = if node.color != [1.0, 1.0, 1.0, 1.0] {
            model.tint_color(node.color)
        } else {
            model
        };
        Some(tinted_model.build())
    }
    Err(e) => { /* ... */ }
}
```

**Alternative Approach**:
- Modify GLTF files on download to include color multiplier
- Use shader modifications via material extensions
- Server-side post-processing of materials

**References**:
- Stardust server: `src/nodes/drawable/model.rs`
- Bevy PBR materials: `bevy::pbr::StandardMaterial`
- GLTF color factors: `material.pbrMetallicRoughness.baseColorFactor`

### 1.2 Texture Application

**Status**: 🟡 Data captured, not applied  
**Difficulty**: High  
**Requires**: Texture download system + material texture binding API

**Current State**:
- Entity texture URLs are parsed from Overte packets ✅
- Texture URLs are stored in bridge state ✅
- Texture URLs are logged ✅
- Textures are NOT downloaded or applied ❌

**What's Needed**:
1. **Texture Downloader** (similar to ModelDownloader):
   ```rust
   pub struct TextureDownloader {
       cache_dir: PathBuf,
       // Similar to ModelDownloader but for images
   }
   
   // Downloads .png, .jpg, .webp, etc.
   fn download_texture(url: &str) -> Option<PathBuf>
   ```

2. **Material Texture Binding**:
   - Load texture as Bevy Image asset
   - Bind to model's material base color texture
   - Handle UV mapping and texture coordinates

3. **Asteroids API Extension** (hypothetical):
   ```rust
   Model::direct(&model_path)
       .base_color_texture(&texture_path)
       .build()
   ```

**Implementation Challenges**:
- GLTF models may already have embedded textures
- Overte textures should override embedded ones
- Need to handle texture tiling/repeat settings
- UV mapping compatibility between Overte and GLTF

**Workaround (Current)**:
- Use GLTF/GLB models with embedded textures
- Entity texture URLs are ignored

### 1.3 Transparency (Alpha) Support

**Status**: 🟡 Data captured, partially working  
**Difficulty**: Low-Medium  
**Requires**: Material alpha mode configuration

**Current State**:
- Entity alpha values parsed ✅
- Alpha stored with color data ✅
- Alpha logged but not applied ❌
- GLTF models can have embedded alpha ✅

**What's Needed**:
- Set material alpha mode based on entity.alpha value
- Configure blending for transparent materials
- Handle alpha testing vs alpha blending

**Implementation**:
```rust
// When alpha < 1.0, configure material for transparency
if node.color[3] < 1.0 {
    // Set alpha mode on material
    // May require material modification API
}
```

## Priority 2: Protocol Support

### 2.1 ATP Protocol (Overte Asset Server)

**Status**: 🔴 Not implemented  
**Difficulty**: High  
**Requires**: AssetClient integration, ATP protocol implementation

**Current State**:
- HTTP/HTTPS URLs download correctly ✅
- file:// URLs work ✅
- atp:// URLs are ignored ❌

**What's Needed**:
1. **ATP Client Implementation**:
   - Connect to Overte asset server
   - Authenticate with asset server credentials
   - Request assets by hash
   - Handle asset caching

2. **Integration with ModelCache**:
   ```cpp
   // In StardustBridge::setNodeModel()
   if (modelUrl.starts_with("atp:")) {
       // Parse ATP URL: atp://hash.format
       // Connect to asset server
       // Download asset
       // Cache locally
       // Pass local path to bridge
   }
   ```

**ATP URL Format**:
```
atp://<hash>.<extension>
Example: atp://8f3e9a1b2c4d5e6f.glb
```

**References**:
- Overte AssetClient: `libraries/networking/src/AssetClient.cpp`
- ATP protocol documentation in Overte source

### 2.2 Entity Script Execution

**Status**: 🔴 Not implemented  
**Difficulty**: Very High  
**Requires**: JavaScript engine, Overte API compatibility layer

**Current State**:
- Entity script URLs are not parsed
- Scripts are not executed
- No script API available

**What's Needed**:
- JavaScript runtime (QuickJS, Deno, or V8)
- Overte entity script API implementation
- Script lifecycle management (load, update, unload)
- Sandboxing for security

**Out of Scope**: This is a massive undertaking and likely not worth it unless there's significant demand for scripted entities.

## Priority 3: Performance

### 3.1 Model Download Optimization

**Status**: 🟡 Works but could be better  
**Difficulty**: Medium

**Current Issues**:
- First render of HTTP models blocks until download completes
- No visual indication of download progress to user
- No retry logic for failed downloads

**Improvements Needed**:
1. **Async Rendering Update**:
   - Render placeholder while downloading
   - Replace with actual model when download completes
   - Requires frame update notification to bridge

2. **Progress Indicators**:
   - Show download progress in XR (progress bar, spinner)
   - Use Stardust UI elements for visual feedback

3. **Retry Logic**:
   - Exponential backoff for failed downloads
   - Max retry count
   - Fallback to primitives after retries exhausted

4. **Parallel Downloads**:
   - Download multiple models simultaneously
   - Connection pooling
   - Rate limiting to avoid overwhelming servers

### 3.2 Cache Management

**Status**: 🟡 Works but unbounded  
**Difficulty**: Low

**Current Issues**:
- Cache grows indefinitely
- No LRU eviction
- No cache size limits
- No cache cleanup on app exit

**Improvements Needed**:
1. **LRU Eviction**:
   - Track last access time for each cached model
   - Remove least recently used when cache exceeds size limit
   - Configurable max cache size (default: 1GB)

2. **Cache Validation**:
   - Check if remote models have been updated (HTTP ETag)
   - Re-download if changed
   - Configurable refresh policy

3. **Manual Cache Management**:
   - CLI command to clear cache
   - UI option in settings
   - Selective deletion (by domain, by age, etc.)

### 3.3 In-Memory Model Caching

**Status**: 🔴 Not implemented  
**Difficulty**: Medium

**Current State**:
- Primitive models loaded from disk every frame
- No in-memory caching of model data
- Redundant I/O for same models

**What's Needed**:
```rust
static MODEL_CACHE: OnceLock<Mutex<HashMap<PathBuf, Handle<Scene>>>> = OnceLock::new();

fn get_cached_model(path: &PathBuf) -> Option<Handle<Scene>> {
    MODEL_CACHE.get()?.lock().unwrap().get(path).cloned()
}
```

**Benefits**:
- Faster rendering for repeated models
- Less disk I/O
- Better frame times

## Priority 4: Entity Type Support

### 4.1 Light Entities

**Status**: 🔴 Not implemented  
**Difficulty**: Medium

**What's Needed**:
- Parse light type (point, spot, directional)
- Parse light color, intensity, range
- Create Stardust PointLight/SpotLight/DirectionalLight nodes
- Position and orient lights correctly

**Stardust API** (likely exists):
```rust
use stardust_xr_asteroids::elements::Light;

Light::point()
    .color([r, g, b])
    .intensity(intensity)
    .range(range)
    .build()
```

### 4.2 Text Entities

**Status**: 🔴 Not implemented  
**Difficulty**: Low-Medium

**What's Needed**:
- Parse text content and font
- Parse text alignment and size
- Create Stardust Text nodes
- Handle line breaks and wrapping

**Stardust API** (exists):
```rust
use stardust_xr_asteroids::elements::Text;

Text::new(content)
    .character_height(size)
    .align_x(XAlign::Center)
    .build()
```

### 4.3 Zone Entities

**Status**: 🔴 Not implemented  
**Difficulty**: Medium

**What's Needed**:
- Parse zone dimensions and shape
- Parse zone properties (ambient light, gravity, etc.)
- Create Stardust zone/spatial with properties
- Handle enter/exit events

**Stardust API**:
```rust
Spatial::default()
    .zoneable(true)
    .zone_radius(radius)
    .build()
```

### 4.4 ParticleEffect Entities

**Status**: 🔴 Not implemented  
**Difficulty**: High

**What's Needed**:
- Parse particle emitter properties
- Create particle system in Stardust
- Handle particle textures and physics
- Synchronize particle state

**Challenge**: Stardust may not have particle system support yet.

## Priority 5: Dynamic Updates

### 5.1 Real-Time Entity Property Updates

**Status**: 🟡 Transform updates work, other properties don't  
**Difficulty**: Medium

**Current State**:
- Transform (position, rotation) updates work ✅
- Color changes not reflected ❌
- Dimension changes not reflected ❌
- Model URL changes not reflected ❌

**What's Needed**:
- Detect property changes in SceneSync
- Call appropriate bridge update functions
- Trigger re-render in Rust bridge when properties change
- Incremental updates instead of full node recreation

**Implementation**:
```cpp
// In SceneSync.cpp
if (it->second.color != e.color) {
    stardust.setNodeColor(nodeId, e.color, e.alpha);
}
if (it->second.dimensions != e.dimensions) {
    stardust.setNodeDimensions(nodeId, e.dimensions);
}
// etc.
```

### 5.2 Physics Synchronization

**Status**: 🔴 Not implemented  
**Difficulty**: Very High

**What's Needed**:
- Sync physics state from Overte
- Apply velocities and forces in Stardust
- Handle collisions and rigid body dynamics
- Keep physics in sync across network

**Challenge**: Stardust physics API is limited. May need custom integration.

## Priority 6: Advanced Features

### 6.1 Avatar Rendering

**Status**: 🔴 Not implemented  
**Difficulty**: Very High

**What's Needed**:
- Parse avatar models and skeletons
- Sync avatar animations
- Handle avatar attachments and wearables
- Render other users' avatars

**Challenge**: Avatar system is complex and Overte-specific.

### 6.2 Spatial Audio

**Status**: 🔴 Not implemented  
**Difficulty**: High

**What's Needed**:
- Connect to Overte audio mixer
- Receive audio streams
- Position audio sources in 3D space
- Mix and play audio in Stardust

**Challenge**: Requires audio mixer protocol implementation.

## Implementation Priority

### Phase 1: Visual Quality (1-2 weeks)
1. Color tinting - Requires asteroids API extension
2. Transparency/alpha - Material configuration
3. Texture support - Download + material binding

### Phase 2: Protocols (2-3 weeks)
1. ATP protocol support
2. Model download optimization
3. Cache management improvements

### Phase 3: Entity Types (1-2 weeks)
1. Light entities
2. Text entities
3. Zone entities (basic)

### Phase 4: Dynamic Features (2-3 weeks)
1. Real-time property updates
2. In-memory caching
3. Progress indicators

### Phase 5: Advanced (4+ weeks)
1. Physics synchronization
2. Avatar rendering
3. Spatial audio
4. Entity scripts

## Getting Help

### To Implement Color Tinting
- Contact Stardust developers about extending asteroids Model API
- Check if material tinting can be done via existing Material API
- Look into shader-based color multiplication

### To Implement ATP Protocol
- Study Overte's AssetClient implementation
- Document ATP packet format
- Create minimal ATP client in C++

### To Implement Physics
- Investigate Stardust's physics capabilities
- Determine if Rapier or other physics engine is available
- Design physics sync protocol

## Conclusion

The core rendering pipeline is **complete and functional**. These enhancements would improve visual fidelity and feature parity, but are not required for basic Overte world viewing in StardustXR.

Prioritize based on user needs:
- **Visual artists**: Color tinting, textures, transparency
- **Content creators**: ATP protocol, advanced entity types
- **Developers**: Physics sync, dynamic updates
- **Users**: Performance, cache management, progress indicators

---

**Document Version**: 1.0  
**Last Updated**: November 9, 2025  
**Status**: Planning Phase
