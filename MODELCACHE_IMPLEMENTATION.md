# ModelCache Implementation - Technical Documentation

## Overview

The `ModelCache` is a C++ singleton class that handles HTTP(S) asset downloading for 3D models in Starworld. It follows Overte's ResourceCache architecture but is implemented in pure C++ using libcurl instead of Qt's networking stack.

**Location:** 
- Header: `src/ModelCache.hpp`
- Implementation: `src/ModelCache.cpp`

**Purpose:**
- Download 3D models from HTTP/HTTPS URLs
- Cache downloaded models to avoid re-downloading
- Provide async callbacks for download progress and completion
- Support model loading via StardustXR's Model::direct(PathBuf)

## Architecture Comparison: Overte vs Starworld

### Overte's ResourceCache Pattern

Overte uses a sophisticated caching system in `libraries/networking/src/`:

```
ResourceCache (base class)
    ├── ResourceRequest (abstract)
    │   ├── HTTPResourceRequest
    │   ├── AssetResourceRequest (ATP protocol)
    │   └── FileResourceRequest
    ├── ModelCache (extends ResourceCache)
    ├── TextureCache (extends ResourceCache)
    └── AnimationCache (extends ResourceCache)
```

**Key Features:**
- Qt-based networking (QNetworkAccessManager, QNetworkReply)
- Resource lifecycle management (loading, loaded, cached, unused)
- Request queueing and priority system
- LRU cache eviction
- ATP protocol support (atp:// URLs)
- Automatic retry with exponential backoff

### Starworld's ModelCache

Our implementation is **simplified but compatible**:

```cpp
ModelCache (singleton)
    ├── Uses libcurl for HTTP downloads
    ├── SHA256-based filename hashing
    ├── Async callbacks (completion, progress)
    ├── Thread-safe resource tracking
    └── Simple state machine (NotStarted → Downloading → Completed/Failed)
```

**Design Decisions:**
1. **No Qt dependency**: Uses libcurl (more lightweight, cross-platform)
2. **C++ only**: Integrates directly into Starworld's C++ codebase
3. **Async via std::thread**: Simple background download threads
4. **SHA256 hashing**: URL → unique filename for cache storage
5. **StardustXR integration**: Returns local paths for Model::direct()

## Implementation Details

### Cache Directory Structure

```
~/.cache/starworld/
├── primitives/          # Blender-generated test models
│   ├── cube.glb         # Red cube (Box entities)
│   ├── sphere.glb       # Green sphere (Sphere entities)
│   └── model.glb        # Blue icosphere (Model placeholder)
└── models/              # Downloaded HTTP models
    ├── <sha256-hash-1>.glb
    ├── <sha256-hash-2>.gltf
    └── <sha256-hash-N>.fbx
```

**Filename Generation:**
```cpp
std::string sha256(const std::string& url);  // SHA256 hash
std::string getExtensionFromUrl(const std::string& url);  // .glb, .gltf, .fbx, .obj
std::string filename = sha256(url) + getExtensionFromUrl(url);
```

### API Usage

```cpp
// Get singleton instance
ModelCache& cache = ModelCache::instance();

// Request a model (async)
cache.requestModel(
    "https://example.com/models/chair.glb",
    
    // Completion callback
    [](const std::string& url, bool success, const std::string& localPath) {
        if (success) {
            std::cout << "Model ready: " << localPath << std::endl;
            // Pass localPath to Model::direct() in Rust bridge
        } else {
            std::cerr << "Download failed: " << url << std::endl;
        }
    },
    
    // Progress callback (optional)
    [](const std::string& url, size_t bytesReceived, size_t bytesTotal) {
        float percent = (bytesReceived * 100.0f) / bytesTotal;
        std::cout << "Downloading: " << percent << "%" << std::endl;
    }
);

// Synchronous checks
if (cache.isCached(url)) {
    std::string path = cache.getCachedPath(url);
}

ModelCache::State state = cache.getState(url);
```

### Integration with StardustBridge

**Before (direct pass-through):**
```cpp
bool StardustBridge::setNodeModel(NodeId id, const std::string& modelUrl) {
    if (m_fnSetModel) {
        return m_fnSetModel(id, modelUrl.c_str()) == 0;
    }
    return true;
}
```

**After (with ModelCache):**
```cpp
bool StardustBridge::setNodeModel(NodeId id, const std::string& modelUrl) {
    // Check if URL is HTTP(S)
    if (modelUrl.substr(0, 7) == "http://" || modelUrl.substr(0, 8) == "https://") {
        // Download via ModelCache, then pass local path to bridge
        ModelCache::instance().requestModel(
            modelUrl,
            [this, id](const std::string& url, bool success, const std::string& localPath) {
                if (success && m_fnSetModel) {
                    m_fnSetModel(id, localPath.c_str());
                }
            }
        );
        return true;  // Download initiated
    }
    
    // Direct URL (file://, atp://, etc.)
    if (m_fnSetModel) {
        return m_fnSetModel(id, modelUrl.c_str()) == 0;
    }
    return true;
}
```

### Thread Safety

The ModelCache uses `std::mutex` to protect shared state:

```cpp
mutable std::mutex mutex_;
std::unordered_map<std::string, std::shared_ptr<ModelResource>> resources_;
std::unordered_map<std::string, std::vector<CompletionCallback>> completionCallbacks_;
std::unordered_map<std::string, std::vector<ProgressCallback>> progressCallbacks_;
```

All public methods acquire the mutex before accessing maps. Downloads run in detached `std::thread` instances.

### Error Handling

**Download failures:**
- Network errors (CURLE_* codes)
- HTTP errors (4xx, 5xx)
- File system errors (can't create cache dir, can't write file)

**Fallback behavior:**
- On download failure, completion callback receives `success=false`
- StardustBridge logs error but doesn't crash
- Rust bridge falls back to primitive models via `get_model_path()`

## Differences from Overte

| Feature | Overte ResourceCache | Starworld ModelCache |
|---------|---------------------|----------------------|
| Networking | Qt (QNetworkAccessManager) | libcurl |
| Threading | Qt event loop | std::thread |
| Caching | LRU with size limits | Simple hash-based (no eviction) |
| Retry logic | Exponential backoff | None (TODO) |
| Progress | QNetworkReply signals | CURL progress callback |
| ATP support | Full AssetClient | Not yet implemented |
| Request queue | Priority-based queue | No queue (immediate download) |
| Cache eviction | LRU with max size | None (grows indefinitely) |

## Future Enhancements

### 1. ATP Protocol Support
Overte's asset server uses `atp://` URLs. To support them:

```cpp
// Map atp:// to http:// using domain asset server info
std::string resolveATPUrl(const std::string& atpUrl, const std::string& assetServerHost) {
    // atp://hash.modelType → http://assetserver:port/hash.modelType
    // Requires AssetClient integration or manual URL construction
}
```

### 2. Request Queueing
Limit concurrent downloads (like Overte's request limit):

```cpp
class ModelCache {
    static constexpr size_t MAX_CONCURRENT = 10;
    std::queue<std::string> pendingDownloads_;
    std::atomic<size_t> activeDownloads_{0};
};
```

### 3. Cache Eviction (LRU)
Track last access time and enforce max cache size:

```cpp
struct ModelResource {
    std::chrono::system_clock::time_point lastAccessed;
    size_t fileSize;
};

void ModelCache::evictLRU(size_t targetSize);
```

### 4. Retry Logic
Implement exponential backoff like Overte:

```cpp
struct ModelResource {
    int attempts = 0;
    static constexpr int MAX_ATTEMPTS = 3;
};

void ModelCache::retryDownload(const std::string& url, int delay_ms);
```

### 5. Content-Type Detection
Use HTTP headers instead of URL heuristics:

```cpp
static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::string header(buffer, size * nitems);
    if (header.find("Content-Type: model/gltf-binary") != std::string::npos) {
        // Use .glb extension
    }
}
```

## Testing

### Unit Tests (TODO)
```cpp
TEST(ModelCache, DownloadHTTP) {
    ModelCache& cache = ModelCache::instance();
    bool completed = false;
    
    cache.requestModel(
        "https://example.com/test.glb",
        [&](const std::string& url, bool success, const std::string& path) {
            EXPECT_TRUE(success);
            EXPECT_TRUE(std::filesystem::exists(path));
            completed = true;
        }
    );
    
    // Wait for async completion
    while (!completed) { std::this_thread::sleep_for(100ms); }
}
```

### Integration Testing
1. **Real Overte server**: Connect to domain with Model entities
2. **Verify downloads**: Check `~/.cache/starworld/models/` for cached files
3. **Check rendering**: Confirm models load via Model::direct() in Rust bridge
4. **Network failure**: Test with unreachable URLs, verify error handling

## Performance Considerations

### Memory
- Each ModelResource is ~1KB (metadata only, not file contents)
- No memory cache (files stay on disk)
- Completed callbacks are cleared after invocation

### Disk I/O
- Downloads write directly to disk (streaming, not buffered in RAM)
- No compression (stores as-downloaded)
- No cache size limit (manual cleanup via `clearCache()`)

### Network
- Parallel downloads (no limit, each in its own thread)
- No request batching
- No connection pooling (libcurl creates new connection per request)

**Optimization TODO:**
- Use curl_multi for connection pooling
- Limit concurrent downloads
- Implement cache size monitoring

## Build Requirements

**CMakeLists.txt additions:**
```cmake
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(stardust-overte-client
    ...
    src/ModelCache.cpp
)

target_link_libraries(stardust-overte-client PRIVATE 
    CURL::libcurl 
    OpenSSL::Crypto
)
```

**System dependencies:**
```bash
# Debian/Ubuntu
sudo apt install libcurl4-openssl-dev libssl-dev

# Fedora
sudo dnf install libcurl-devel openssl-devel

# Arch
sudo pacman -S curl openssl
```

## References

- Overte ResourceCache: `overte/libraries/networking/src/ResourceCache.{h,cpp}`
- Overte ModelCache: `overte/libraries/model-networking/src/model-networking/ModelCache.{h,cpp}`
- Overte HTTPResourceRequest: `overte/libraries/networking/src/HTTPResourceRequest.cpp`
- libcurl documentation: https://curl.se/libcurl/c/
- OpenSSL SHA256: https://www.openssl.org/docs/man1.1.1/man3/SHA256.html

## Summary

The ModelCache successfully implements HTTP asset downloading for Starworld, following Overte's architectural patterns while using modern C++ and standard libraries. It provides async model loading with caching, integrating seamlessly with the StardustXR bridge via local file paths.

**Key Achievement:** Phase 2 of the roadmap (Asset Pipeline) is now complete! ✅
