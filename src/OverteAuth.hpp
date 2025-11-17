// OverteAuth.hpp
#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

// Forward declaration
class RSAKeypair;

// Simple OAuth2 authentication for Overte metaverse
class OverteAuth {
public:
    OverteAuth();
    ~OverteAuth();
    
    // Authenticate with username/password (Resource Owner Password Grant)
    bool login(const std::string& username, const std::string& password, 
               const std::string& metaverseUrl = "https://mv.overte.org");
    
    // Authenticate with browser OAuth flow (Authorization Code Grant) - RECOMMENDED
    bool loginWithBrowser(const std::string& metaverseUrl = "https://mv.overte.org");
    
    // Authenticate with authorization code (after browser callback)
    bool loginWithAuthCode(const std::string& authCode, const std::string& redirectUri);
    
    // Refresh access token
    bool refreshAccessToken();
    
    // Check if we have a valid access token
    bool isAuthenticated() const { return !m_accessToken.empty() && !isTokenExpired(); }
    
    // Get current access token
    const std::string& getAccessToken() const { return m_accessToken; }
    
    // Get username
    const std::string& getUsername() const { return m_username; }
    
    // Get last error message
    const std::string& getLastError() const { return m_lastError; }
    
    // Logout
    void logout();
    
    // Token persistence
    bool loadTokenFromFile();
    bool saveTokenToFile();
    
    // RSA Keypair management (for username signature authentication)
    bool generateKeypair();
    bool uploadPublicKey();
    bool hasKeypair() const;
    std::vector<uint8_t> getUsernameSignature(const std::string& connectionToken) const;
    
private:
    std::string m_metaverseUrl;
    std::string m_accessToken;
    std::string m_refreshToken;
    std::string m_username;
    std::uint64_t m_tokenExpiresAt{0}; // Unix timestamp in seconds
    std::string m_lastError;
    std::string m_clientId = "starworld";
    std::string m_clientSecret = ""; // Public client
    
    // RSA keypair for signature authentication
    std::unique_ptr<RSAKeypair> m_keypair;
    
    // OAuth callback HTTP server
    int m_callbackServerFd = -1;
    int m_callbackPort = 0;
    std::atomic<bool> m_callbackRunning{false};
    std::unique_ptr<std::thread> m_callbackThread;
    std::string m_receivedAuthCode;
    std::string m_authState; // CSRF protection
    
    // Helper methods
    bool isTokenExpired() const;
    bool needsRefresh() const; // Returns true if token expires within 1 hour
    std::string getTokenFilePath();
    std::string getConfigDir();
    bool parseTokenResponse(const std::string& jsonResponse);
    std::string generateRandomState();
    std::string urlEncode(const std::string& value);
    bool openBrowser(const std::string& url);
    
    // HTTP helpers
    bool httpPost(const std::string& url, const std::string& postData, std::string& response);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static std::string extractJsonString(const std::string& json, const std::string& key);
    static std::uint64_t extractJsonInt(const std::string& json, const std::string& key);
    
    // OAuth callback server
    bool startCallbackServer();
    void stopCallbackServer();
    void callbackServerThread();
    void handleCallbackRequest(int clientFd);
    std::string getCallbackURL() const;
};

