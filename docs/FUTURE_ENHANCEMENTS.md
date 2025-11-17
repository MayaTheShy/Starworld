
# Future Enhancements - Overte to Stardust Entity Rendering

## Overview

All core functionality for basic 3D model rendering is implemented and stable. The following enhancements would improve visual fidelity and feature parity with native Overte. See [`docs/IMPLEMENTATION_COMPLETE.md`](IMPLEMENTATION_COMPLETE.md) and [`docs/ENTITY_TROUBLESHOOTING.md`](ENTITY_TROUBLESHOOTING.md) for current status and limitations.


## Visual Fidelity

- **Color Tinting for Models**: Data is parsed, stored, and logged, but not visually applied (pending StardustXR asteroids API extension).
- **Texture Application**: Texture URLs are parsed, textures are downloaded and cached, but not visually applied (pending StardustXR material API).
- **Transparency (Alpha) Support**: Alpha values are parsed and stored, but not visually applied (pending material API support).

atp://<hash>.<extension>

## Protocol Support

- **ATP Protocol (Overte Asset Server)**: Not implemented. Use HTTP URLs for now. atp:// support requires AssetClient integration.
- **Entity Script Execution**: Not implemented. Out of scope unless there is significant demand.


## Performance

- **Model Download Optimization**: HTTP/HTTPS download and caching is implemented. Async rendering and progress indicators are not yet implemented.
- **Cache Management**: Cache grows indefinitely; LRU eviction and manual management are not yet implemented.
- **In-Memory Model Caching**: Not implemented; would improve performance for repeated models.


## Entity Type Support

- **Light, Text, Zone, ParticleEffect Entities**: Not implemented. Only Box, Sphere, Model are currently supported.


## Dynamic Updates

- **Real-Time Entity Property Updates**: Transform updates work; other property changes (color, dimension, model URL) are not yet reflected in real time.
- **Physics Synchronization**: Not implemented; would require Stardust physics API integration.


## Advanced Features

- **Avatar Rendering**: Not implemented.
- **Spatial Audio**: Not implemented.


## Implementation Priority

See [`docs/IMPLEMENTATION_COMPLETE.md`](IMPLEMENTATION_COMPLETE.md) for current status and priorities.


## Getting Help

See [`docs/ENTITY_TROUBLESHOOTING.md`](ENTITY_TROUBLESHOOTING.md) for troubleshooting and [`docs/IMPLEMENTATION_COMPLETE.md`](IMPLEMENTATION_COMPLETE.md) for implementation details. For API extensions, contact StardustXR developers.


## Conclusion

The core rendering pipeline is complete and functional. Enhancements listed here would improve visual fidelity and feature parity, but are not required for basic Overte world viewing in StardustXR. See [`docs/IMPLEMENTATION_COMPLETE.md`](IMPLEMENTATION_COMPLETE.md) for the latest status.
