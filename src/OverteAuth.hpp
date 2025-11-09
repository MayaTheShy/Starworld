// OverteAuth.hpp
#pragma once

#include <string>
#include <functional>

// Simple OAuth2 authentication for Overte metaverse
class OverteAuth {
public:
    OverteAuth();
    ~OverteAuth();
    
    // Authenticate with username/password
    bool login(const std::string& username, const std::string& password, 
               const std::string& metaverseUrl = "https://mv.overte.org");
    
    // Check if we have a valid access token
    bool isAuthenticated() const { return !m_accessToken.empty(); }
    
    // Get current access token
    const std::string& getAccessToken() const { return m_accessToken; }
    
    // Get username
    const std::string& getUsername() const { return m_username; }
    
    // Logout
    void logout();
    
private:
    std::string m_metaverseUrl;
    std::string m_accessToken;
    std::string m_refreshToken;
    std::string m_username;
    std::uint64_t m_tokenExpiresAt{0}; // Unix timestamp
    
    // libcurl callback for writing response data
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    
    // Parse JSON response (simple key-value extraction)
    static std::string extractJsonString(const std::string& json, const std::string& key);
};
