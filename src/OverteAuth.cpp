// OverteAuth.cpp
#include "OverteAuth.hpp"

#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <curl/curl.h>

using namespace std::chrono;

OverteAuth::OverteAuth() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

OverteAuth::~OverteAuth() {
    curl_global_cleanup();
}

size_t OverteAuth::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string OverteAuth::extractJsonString(const std::string& json, const std::string& key) {
    // Simple JSON string extraction (not a full parser, but works for our needs)
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) {
        return "";
    }
    
    // Find the opening quote after the colon
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return "";
    }
    
    size_t quoteStart = json.find('"', colonPos);
    if (quoteStart == std::string::npos) {
        return "";
    }
    
    // Find the closing quote
    size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) {
        return "";
    }
    
    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

bool OverteAuth::login(const std::string& username, const std::string& password, const std::string& metaverseUrl) {
    m_metaverseUrl = metaverseUrl;
    m_username = username;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[OverteAuth] Failed to initialize CURL" << std::endl;
        return false;
    }
    
    // Construct OAuth token endpoint
    std::string tokenUrl = m_metaverseUrl + "/oauth/token";
    
    // Construct POST data
    std::ostringstream postData;
    postData << "grant_type=password";
    postData << "&username=" << username;  // Should URL-encode but simple for now
    postData << "&password=" << password;
    postData << "&scope=owner";
    
    std::string responseData;
    
    curl_easy_setopt(curl, CURLOPT_URL, tokenUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Starworld/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Set Content-Type header
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "[OverteAuth] Login failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    if (httpCode != 200) {
        std::cerr << "[OverteAuth] Login failed with HTTP " << httpCode << std::endl;
        std::cerr << "[OverteAuth] Response: " << responseData << std::endl;
        return false;
    }
    
    // Parse JSON response
    m_accessToken = extractJsonString(responseData, "access_token");
    m_refreshToken = extractJsonString(responseData, "refresh_token");
    
    if (m_accessToken.empty()) {
        std::cerr << "[OverteAuth] No access token in response" << std::endl;
        return false;
    }
    
    // Set expiration time (default to 1 hour if not specified)
    auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    m_tokenExpiresAt = now + 3600;
    
    std::cout << "[OverteAuth] Successfully authenticated as " << username << std::endl;
    std::cout << "[OverteAuth] Access token: " << m_accessToken.substr(0, 20) << "..." << std::endl;
    
    return true;
}

void OverteAuth::logout() {
    m_accessToken.clear();
    m_refreshToken.clear();
    m_username.clear();
    m_tokenExpiresAt = 0;
    
    std::cout << "[OverteAuth] Logged out" << std::endl;
}
