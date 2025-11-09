// ModelCache.hpp
// Manages downloading and caching of 3D models from HTTP/HTTPS URLs
#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <filesystem>

namespace fs = std::filesystem;

class ModelCache {
public:
    enum class State {
        NotStarted,
        Downloading,
        Completed,
        Failed
    };

    struct ModelResource {
        std::string url;
        fs::path localPath;
        State state = State::NotStarted;
        size_t bytesReceived = 0;
        size_t bytesTotal = 0;
        std::string errorMessage;
    };

    using ProgressCallback = std::function<void(const std::string& url, size_t bytesReceived, size_t bytesTotal)>;
    using CompletionCallback = std::function<void(const std::string& url, bool success, const std::string& localPath)>;

    static ModelCache& instance();

    // Request a model from URL. If already cached, returns path immediately via callback.
    // Otherwise, starts download and calls callback when complete.
    void requestModel(const std::string& url, 
                      CompletionCallback onComplete,
                      ProgressCallback onProgress = nullptr);

    // Synchronous check if model is already cached
    bool isCached(const std::string& url) const;
    
    // Get local path if cached (empty string if not)
    std::string getCachedPath(const std::string& url) const;

    // Get current state of a model request
    State getState(const std::string& url) const;

    // Clear all cached models
    void clearCache();

    // Set cache directory (default: ~/.cache/starworld/models/)
    void setCacheDirectory(const fs::path& dir);
    fs::path getCacheDirectory() const { return cacheDir_; }

private:
    ModelCache();
    ~ModelCache() = default;
    ModelCache(const ModelCache&) = delete;
    ModelCache& operator=(const ModelCache&) = delete;

    // Generate cache filename from URL (using hash)
    std::string urlToFilename(const std::string& url) const;
    
    // Start actual download (runs in background thread)
    void startDownload(const std::string& url);

    // Handle download completion
    void onDownloadComplete(const std::string& url, bool success, const std::string& error = "");

    mutable std::mutex mutex_;
    fs::path cacheDir_;
    std::unordered_map<std::string, std::shared_ptr<ModelResource>> resources_;
    
    // Callbacks stored per URL
    std::unordered_map<std::string, std::vector<CompletionCallback>> completionCallbacks_;
    std::unordered_map<std::string, std::vector<ProgressCallback>> progressCallbacks_;
};
