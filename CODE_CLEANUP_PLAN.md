# Code Cleanup Plan

## Issues Identified

### 1. **Anonymous Namespaces vs Static Functions**
Current state is inconsistent:
- `DomainDiscovery.cpp`: Uses `namespace {}` for helpers ✅ Good
- `ModelCache.cpp`: Uses `namespace {}` for helpers ✅ Good
- `OverteClient.cpp`: Uses `namespace {}` for helpers ✅ Good
- `StardustBridge.cpp`: Uses `static` functions ❌ Should use namespace
- `NLPacketCodec.cpp`: Uses `static` constants and functions ❌ Mixed approach

**Fix**: Standardize on anonymous namespaces for internal implementation details.

### 2. **Redundant Includes**
- `DomainDiscovery.cpp`: Includes `<string>` and `<vector>` (already in header)
- Multiple files include `<iostream>` just for debug logging

**Fix**: Remove redundant includes, only include what's needed.

### 3. **Unused Functions** (to verify)
- `StardustBridge::defaultSocketPath()` - Only used internally, should be private or in anonymous namespace
- `parseDomainsFromJson()` in DomainDiscovery - Marked "for tests" but tests don't use it yet
- Various static parsing helpers in NLPacketCodec

**Fix**: Move unused public functions to private/internal, or remove if truly unused.

### 4. **Header Organization**
Current headers are well-organized, but some improvements:
- `OverteNetworkClient.hpp` - Empty interface, should document why
- Forward declarations could reduce includes in headers

### 5. **Constants in .cpp Files**
Good practice currently:
- `NLPacketCodec.cpp`: Has packet type constants ✅
- `ModelCache.cpp`: Has helper functions in namespace ✅
- Should continue this pattern

## Cleanup Tasks

### Task 1: Standardize Anonymous Namespaces
**Files**: StardustBridge.cpp, NLPacketCodec.cpp

Replace:
```cpp
static std::vector<std::string> candidateSocketPaths() { ... }
```

With:
```cpp
namespace {
std::vector<std::string> candidateSocketPaths() { ... }
} // anonymous namespace
```

### Task 2: Remove Redundant Includes
**All .cpp files**

Before:
```cpp
#include "Header.hpp"
#include <string>  // Redundant if in header
#include <vector>  // Redundant if in header
```

After:
```cpp
#include "Header.hpp"
// Only includes not in header
```

### Task 3: Move Internal Functions
**StardustBridge.cpp/hpp**

Move `defaultSocketPath()` to private or make it a free function in anonymous namespace since it's only used in `connect()`.

### Task 4: Document Unused Exports
**DomainDiscovery.hpp**

Either:
- Add tests that use `parseDomainsFromJson()`, or
- Move it to anonymous namespace if not needed externally

### Task 5: Consolidate Constants
**NLPacketCodec.cpp**

The static constants at top are fine, but could group related ones better:

```cpp
namespace {
// Packet header bit masks
constexpr uint32_t CONTROL_BIT_MASK = 0x80000000;
constexpr uint32_t RELIABLE_BIT_MASK = 0x40000000;
// ... group logically
} // anonymous namespace
```

## Files to Modify

1. ✅ `src/StardustBridge.cpp` - Use anonymous namespace, move internal helpers
2. ✅ `src/StardustBridge.hpp` - Remove or make defaultSocketPath() private
3. ✅ `src/NLPacketCodec.cpp` - Use anonymous namespace for helpers
4. ✅ `src/DomainDiscovery.cpp` - Remove redundant includes
5. ✅ `src/DomainDiscovery.hpp` - Document or remove parseDomainsFromJson
6. ✅ `src/ModelCache.cpp` - Remove redundant includes
7. ✅ `src/OverteClient.cpp` - Remove redundant includes

## Non-Issues (Keep As-Is)

- ✅ `SceneSync` - Clean static class pattern
- ✅ `InputHandler` - Simple and clean
- ✅ `ModelCache` - Well-organized with anonymous namespace
- ✅ Header guards using `#pragma once`
- ✅ Modern C++ patterns (std::optional, smart pointers)

## Estimated Impact

- **Code reduction**: ~20-30 lines (redundant includes)
- **Consistency**: All internal helpers in anonymous namespaces
- **Readability**: Clear separation of public API vs implementation
- **No behavior changes**: Pure refactoring

## Order of Operations

1. Start with StardustBridge (most impact)
2. NLPacketCodec (many static items)
3. Clean up includes across all files
4. Document remaining design decisions
