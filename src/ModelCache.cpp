// ModelCache.cpp
#include "ModelCache.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <cstring>

// For HTTP downloads - using libcurl (cross-platform)
#include <curl/curl.h>

// For hashing URLs to filenames
#include <openssl/sha.h>

namespace {
    // CURL write callback
    size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t totalSize = size * nmemb;
        auto* stream = static_cast<std::ofstream*>(userp);
        stream->write(static_cast<const char*>(contents), totalSize);
        return totalSize;
    }

    // CURL progress callback
    int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        auto* callbacks = static_cast<std::vector<ModelCache::ProgressCallback>*>(clientp);
        if (callbacks && !callbacks->empty()) {
            // Note: We'd need to pass URL here, but CURL doesn't make that easy
            // For now, just track progress internally
        }
        return 0; // Return 0 to continue download
    }

    // Convert URL to SHA256 hash for filename
    std::string sha256(const std::string& str) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(str.c_str()), str.length(), hash);
        
        std::ostringstream oss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return oss.str();
    }

    // Determine file extension from URL or Content-Type
    std::string getExtensionFromUrl(const std::string& url) {
        // Simple heuristic: look for common 3D model extensions
        if (url.find(".glb") != std::string::npos || url.find(".GLB") != std::string::npos) {
            return ".glb";
        } else if (url.find(".gltf") != std::string::npos || url.find(".GLTF") != std::string::npos) {
            return ".gltf";
        } else if (url.find(".fbx") != std::string::npos || url.find(".FBX") != std::string::npos) {
            return ".fbx";
        } else if (url.find(".obj") != std::string::npos || url.find(".OBJ") != std::string::npos) {
            return ".obj";
        }
        // Default to GLB for Overte compatibility
        return ".glb";
    }
}

ModelCache& ModelCache::instance() {
    static ModelCache instance;
    return instance;
}

ModelCache::ModelCache() {
    // Initialize libcurl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Set default cache directory: ~/.cache/starworld/models/
    const char* home = std::getenv("HOME");
    if (home) {
        cacheDir_ = fs::path(home) / ".cache" / "starworld" / "models";
    } else {
        cacheDir_ = fs::path("/tmp") / "starworld" / "models";
    }
    
    // Create cache directory if it doesn't exist
    try {
        fs::create_directories(cacheDir_);
        std::cout << "[ModelCache] Cache directory: " << cacheDir_ << std::endl;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[ModelCache] Failed to create cache directory: " << e.what() << std::endl;
    }
}

void ModelCache::setCacheDirectory(const fs::path& dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    cacheDir_ = dir;
    try {
        fs::create_directories(cacheDir_);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[ModelCache] Failed to create cache directory: " << e.what() << std::endl;
    }
}

std::string ModelCache::urlToFilename(const std::string& url) const {
    // Hash the URL to get a unique filename
    std::string hash = sha256(url);
    std::string ext = getExtensionFromUrl(url);
    return hash + ext;
}

bool ModelCache::isCached(const std::string& url) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string filename = urlToFilename(url);
    fs::path localPath = cacheDir_ / filename;
    
    return fs::exists(localPath) && fs::is_regular_file(localPath);
}

std::string ModelCache::getCachedPath(const std::string& url) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string filename = urlToFilename(url);
    fs::path localPath = cacheDir_ / filename;
    
    if (fs::exists(localPath) && fs::is_regular_file(localPath)) {
        return localPath.string();
    }
    return "";
}

ModelCache::State ModelCache::getState(const std::string& url) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = resources_.find(url);
    if (it != resources_.end()) {
        return it->second->state;
    }
    return State::NotStarted;
}

void ModelCache::requestModel(const std::string& url, 
                              CompletionCallback onComplete,
                              ProgressCallback onProgress) {
    // Check if already cached
    if (isCached(url)) {
        std::string cachedPath = getCachedPath(url);
        std::cout << "[ModelCache] Using cached model: " << url << " -> " << cachedPath << std::endl;
        if (onComplete) {
            onComplete(url, true, cachedPath);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if download is already in progress
        auto it = resources_.find(url);
        if (it != resources_.end()) {
            // Download already in progress, just add callbacks
            if (onComplete) {
                completionCallbacks_[url].push_back(onComplete);
            }
            if (onProgress) {
                progressCallbacks_[url].push_back(onProgress);
            }
            std::cout << "[ModelCache] Download already in progress: " << url << std::endl;
            return;
        }

        // Create new resource entry
        auto resource = std::make_shared<ModelResource>();
        resource->url = url;
        resource->localPath = cacheDir_ / urlToFilename(url);
        resource->state = State::Downloading;
        resources_[url] = resource;
        
        // Store callbacks
        if (onComplete) {
            completionCallbacks_[url].push_back(onComplete);
        }
        if (onProgress) {
            progressCallbacks_[url].push_back(onProgress);
        }
    }

    // Start download in background thread
    std::cout << "[ModelCache] Starting download: " << url << std::endl;
    std::thread([this, url]() {
        this->startDownload(url);
    }).detach();
}

void ModelCache::startDownload(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[ModelCache] Failed to initialize CURL for: " << url << std::endl;
        onDownloadComplete(url, false, "Failed to initialize CURL");
        return;
    }

    std::shared_ptr<ModelResource> resource;
    fs::path localPath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(url);
        if (it == resources_.end()) {
            curl_easy_cleanup(curl);
            return;
        }
        resource = it->second;
        localPath = resource->localPath;
    }

    // Open output file
    std::ofstream outFile(localPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "[ModelCache] Failed to open output file: " << localPath << std::endl;
        curl_easy_cleanup(curl);
        onDownloadComplete(url, false, "Failed to open output file");
        return;
    }

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Starworld/1.0 (Overte Client for StardustXR)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Progress tracking
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    
    // Perform download
    CURLcode res = curl_easy_perform(curl);
    outFile.close();

    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        std::cerr << "[ModelCache] Download failed: " << url << " - " << error << std::endl;
        
        // Clean up failed download
        try {
            fs::remove(localPath);
        } catch (...) {}
        
        curl_easy_cleanup(curl);
        onDownloadComplete(url, false, error);
        return;
    }

    // Check HTTP response code
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    if (httpCode >= 400) {
        std::cerr << "[ModelCache] HTTP error " << httpCode << " for: " << url << std::endl;
        
        try {
            fs::remove(localPath);
        } catch (...) {}
        
        curl_easy_cleanup(curl);
        onDownloadComplete(url, false, "HTTP error " + std::to_string(httpCode));
        return;
    }

    // Get download size
    curl_off_t downloadSize = 0;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloadSize);
    
    curl_easy_cleanup(curl);

    std::cout << "[ModelCache] Download complete: " << url << " (" << downloadSize << " bytes) -> " << localPath << std::endl;
    onDownloadComplete(url, true);
}

void ModelCache::onDownloadComplete(const std::string& url, bool success, const std::string& error) {
    std::vector<CompletionCallback> callbacks;
    std::string localPath;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = resources_.find(url);
        if (it != resources_.end()) {
            it->second->state = success ? State::Completed : State::Failed;
            if (!error.empty()) {
                it->second->errorMessage = error;
            }
            localPath = it->second->localPath.string();
        }
        
        // Get callbacks
        auto cbIt = completionCallbacks_.find(url);
        if (cbIt != completionCallbacks_.end()) {
            callbacks = std::move(cbIt->second);
            completionCallbacks_.erase(cbIt);
        }
        
        // Clear progress callbacks
        progressCallbacks_.erase(url);
    }

    // Call all completion callbacks
    for (auto& cb : callbacks) {
        if (cb) {
            cb(url, success, success ? localPath : "");
        }
    }
}

void ModelCache::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Remove all files in cache directory
        for (const auto& entry : fs::directory_iterator(cacheDir_)) {
            if (entry.is_regular_file()) {
                fs::remove(entry.path());
            }
        }
        std::cout << "[ModelCache] Cache cleared" << std::endl;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[ModelCache] Failed to clear cache: " << e.what() << std::endl;
    }
    
    resources_.clear();
    completionCallbacks_.clear();
    progressCallbacks_.clear();
}
