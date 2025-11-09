# Directory Structure

## Root Directory Layout

```
Starworld/
├── README.md                # Main project documentation
├── LICENSE                  # Project license
├── CMakeLists.txt          # CMake build configuration
├── build_and_test.sh       # Convenience wrapper (points to scripts/)
│
├── src/                    # C++ source code
├── bridge/                 # Rust StardustXR bridge
├── tests/                  # C++ test suite
├── tools/                  # Python utilities (Blender export, etc.)
│
├── docs/                   # All documentation files
├── scripts/                # Build and utility scripts
├── examples/               # Example files and test data
│
├── build/                  # CMake build output (gitignored)
└── third_party/            # Vendored dependencies (optional)
```

## Directory Purposes

### `/src`
C++ source code for the Overte client:
- `main.cpp` - Entry point
- `OverteClient.cpp` - Overte protocol implementation
- `StardustBridge.cpp` - C++/Rust bridge interface
- `SceneSync.cpp` - Entity synchronization
- `NLPacketCodec.cpp` - Packet encoding/decoding
- `QDataStream.cpp` - Qt serialization

### `/bridge`
Rust implementation of StardustXR client:
- `src/lib.rs` - Bridge implementation with C ABI
- `Cargo.toml` - Rust dependencies
- Compiled to `libstardust_bridge.so`

### `/tests`
C++ test harness:
- `TestHarness.cpp` - Unit tests
- `CMakeLists.txt` - Test build configuration

### `/tools`
Python utilities:
- `blender_export_simple.py` - Generate primitive models
- Other development tools

### `/docs` (NEW)
All project documentation:
- `README.md` - Documentation index
- `DEVELOPER_GUIDE.md` - Quick reference
- `CHANGELOG.md` - Version history
- `OVERTE_*.md` - Overte protocol docs
- `*_IMPLEMENTATION.md` - Implementation details
- `CI_SETUP_SUMMARY.md` - CI/CD documentation

### `/scripts` (NEW)
Build and utility scripts:
- `build_and_test.sh` - Full clean build and test
- `ci-test.sh` - CI test script
- `run_with_auth.sh` - Run with authentication (future)

### `/examples` (NEW)
Example files and test data:
- `test_entities.json` - Sample entity configuration
- `primitives/` - Pre-built primitive models (cube, sphere)

### `/third_party`
Vendored dependencies (optional):
- StardustXR crates (core, asteroids)
- Other dependencies for deterministic builds

### `/build`
CMake build output (not in git):
- `starworld` - Main executable
- `starworld-tests` - Test executable
- `*.o` - Object files

## File Organization Principles

### Root Directory (Keep Clean!)
Only essential files in the root:
- Main documentation (README.md)
- Build configuration (CMakeLists.txt)
- License
- Optional: Top-level convenience scripts

### Documentation
All `.md` files (except README) go in `/docs`:
- Organized by topic
- Linked from main README
- Cross-referenced with relative links

### Scripts
All `.sh` scripts go in `/scripts`:
- Build scripts
- Test scripts
- Utility scripts
- Development helpers

### Examples
Test data and examples go in `/examples`:
- Sample configurations
- Test models
- Example entities

## Migrating Old Paths

If you have scripts or documentation referencing old paths:

### Documentation Files
```
OLD: ./DEVELOPER_GUIDE.md
NEW: ./docs/DEVELOPER_GUIDE.md

OLD: ./OVERTE_AUTH.md  
NEW: ./docs/OVERTE_AUTH.md
```

### Scripts
```
OLD: ./build_and_test.sh (still works as wrapper)
NEW: ./scripts/build_and_test.sh (actual script)

OLD: ./ci-test.sh
NEW: ./scripts/ci-test.sh
```

### Examples
```
OLD: ./test_entities.json
NEW: ./examples/test_entities.json

OLD: ./primitives/
NEW: ./examples/primitives/
```

## Benefits of New Structure

1. **Cleaner Root**: Easy to see what the project is about
2. **Logical Grouping**: Similar files together
3. **Scalability**: Easy to add new docs/scripts/examples
4. **Navigation**: Clear where to find things
5. **Professional**: Matches common open-source project layouts

## Comparison

### Before
```
Starworld/
├── README.md
├── DEVELOPER_GUIDE.md
├── CHANGELOG.md
├── OVERTE_AUTH.md
├── OVERTE_ASSIGNMENT_CLIENT_TASK.md
├── ENTITY_RENDERING_ENHANCEMENTS.md
├── MODELCACHE_IMPLEMENTATION.md
├── CI_SETUP_SUMMARY.md
├── CODE_CLEANUP_PLAN.md
├── build_and_test.sh
├── ci-test.sh
├── run_with_auth.sh
├── test_entities.json
├── primitives/
├── CMakeLists.txt
├── LICENSE
├── src/
├── bridge/
├── tests/
├── tools/
└── third_party/
```

### After
```
Starworld/
├── README.md
├── CMakeLists.txt
├── LICENSE
├── build_and_test.sh    (wrapper)
├── src/
├── bridge/
├── tests/
├── tools/
├── docs/                (8 .md files)
├── scripts/             (3 .sh files)
├── examples/            (test data, primitives)
└── third_party/
```

Much better! 🎉
