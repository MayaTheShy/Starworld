// OverteAuth.cpp - Enhanced OAuth implementation with browser flow
#include "OverteAuth.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <chrono>
#include <random>
#include <iomanip>
#include <curl/curl.h>

// For HTTP callback server
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

// For file paths
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

using namespace std::chrono;

OverteAuth::OverteAuth() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Try to load saved token
    loadTokenFromFile();
}

OverteAuth::~OverteAuth() {
    stopCallbackServer();
    curl_global_cleanup();
}

// ============================================================================
// HTTP Helpers
// ============================================================================

size_t OverteAuth::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string OverteAuth::extractJsonString(const std::string& json, const std::string& key) {
    // Simple JSON string extraction
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t quoteStart = json.find('"', colonPos);
    if (quoteStart == std::string::npos) return "";
    
    size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";
    
    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

std::uint64_t OverteAuth::extractJsonInt(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return 0;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return 0;
    
    size_t numStart = colonPos + 1;
    while (numStart < json.size() && (json[numStart] == ' ' || json[numStart] == '\t')) {
        numStart++;
    }
    
    size_t numEnd = numStart;
    while (numEnd < json.size() && (json[numEnd] >= '0' && json[numEnd] <= '9')) {
        numEnd++;
    }
    
    if (numEnd == numStart) return 0;
    
    try {
        return std::stoull(json.substr(numStart, numEnd - numStart));
    } catch (...) {
        return 0;
    }
}

std::string OverteAuth::urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int((unsigned char) c);
        }
    }
    
    return escaped.str();
}

std::string OverteAuth::generateRandomState() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex_chars = "0123456789abcdef";
    std::string state;
    for (int i = 0; i < 32; i++) {
        state += hex_chars[dis(gen)];
    }
    return state;
}

bool OverteAuth::httpPost(const std::string& url, const std::string& postData, std::string& response) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        m_lastError = "Failed to initialize CURL";
        return false;
    }
    
    response.clear();
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Starworld/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        m_lastError = std::string("CURL error: ") + curl_easy_strerror(res);
        return false;
    }
    
    if (httpCode < 200 || httpCode >= 300) {
        m_lastError = "HTTP error " + std::to_string(httpCode) + ": " + response;
        return false;
    }
    
    return true;
}

bool OverteAuth::openBrowser(const std::string& url) {
    std::cout << "[OverteAuth] Opening browser to: " << url << std::endl;
    
    // Try xdg-open (Linux standard)
    std::string command = "xdg-open '" + url + "' >/dev/null 2>&1 &";
    int result = system(command.c_str());
    
    if (result != 0) {
        // Fallback attempts
        command = "x-www-browser '" + url + "' >/dev/null 2>&1 &";
        result = system(command.c_str());
    }
    
    if (result != 0) {
        m_lastError = "Failed to open browser. Please manually navigate to: " + url;
        std::cerr << "[OverteAuth] " << m_lastError << std::endl;
        return false;
    }
    
    return true;
}

// ============================================================================
// Token Management
// ============================================================================

bool OverteAuth::isTokenExpired() const {
    auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    return now >= m_tokenExpiresAt;
}

bool OverteAuth::needsRefresh() const {
    auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    return (m_tokenExpiresAt - now) < 3600; // Less than 1 hour remaining
}

bool OverteAuth::parseTokenResponse(const std::string& jsonResponse) {
    m_accessToken = extractJsonString(jsonResponse, "access_token");
    m_refreshToken = extractJsonString(jsonResponse, "refresh_token");
    
    if (m_accessToken.empty()) {
        m_lastError = "No access token in response";
        std::string error = extractJsonString(jsonResponse, "error");
        if (!error.empty()) {
            std::string errorDesc = extractJsonString(jsonResponse, "error_description");
            m_lastError = error + ": " + errorDesc;
        }
        return false;
    }
    
    // Extract expiration
    std::uint64_t expiresIn = extractJsonInt(jsonResponse, "expires_in");
    if (expiresIn == 0) {
        expiresIn = 3600; // Default to 1 hour
    }
    
    auto now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    m_tokenExpiresAt = now + expiresIn;
    
    std::cout << "[OverteAuth] Token received, expires in " << expiresIn << " seconds" << std::endl;
    std::cout << "[OverteAuth] Access token: " << m_accessToken.substr(0, 20) << "..." << std::endl;
    
    // Save to file
    saveTokenToFile();
    
    return true;
}

std::string OverteAuth::getConfigDir() {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw->pw_dir;
    }
    
    std::string configDir = std::string(home) + "/.config/starworld";
    
    // Create directory if it doesn't exist
    mkdir(configDir.c_str(), 0700);
    
    return configDir;
}

std::string OverteAuth::getTokenFilePath() {
    return getConfigDir() + "/overte_token.txt";
}

bool OverteAuth::loadTokenFromFile() {
    std::string filepath = getTokenFilePath();
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        return false; // File doesn't exist yet
    }
    
    std::string line;
    std::getline(file, m_metaverseUrl);
    std::getline(file, m_username);
    std::getline(file, m_accessToken);
    std::getline(file, m_refreshToken);
    
    if (std::getline(file, line)) {
        try {
            m_tokenExpiresAt = std::stoull(line);
        } catch (...) {
            m_tokenExpiresAt = 0;
        }
    }
    
    file.close();
    
    if (m_accessToken.empty()) {
        return false;
    }
    
    std::cout << "[OverteAuth] Loaded saved token for " << m_username << std::endl;
    
    // Check if token needs refresh
    if (isTokenExpired()) {
        std::cout << "[OverteAuth] Token expired, attempting refresh..." << std::endl;
        return refreshAccessToken();
    } else if (needsRefresh()) {
        std::cout << "[OverteAuth] Token expiring soon, refreshing..." << std::endl;
        refreshAccessToken(); // Best effort, don't fail if this doesn't work
    }
    
    return !m_accessToken.empty();
}

bool OverteAuth::saveTokenToFile() {
    std::string filepath = getTokenFilePath();
    std::ofstream file(filepath);
    
    if (!file.is_open()) {
        m_lastError = "Failed to save token to " + filepath;
        return false;
    }
    
    file << m_metaverseUrl << "\n";
    file << m_username << "\n";
    file << m_accessToken << "\n";
    file << m_refreshToken << "\n";
    file << m_tokenExpiresAt << "\n";
    
    file.close();
    
    // Set file permissions to user-only
    chmod(filepath.c_str(), 0600);
    
    std::cout << "[OverteAuth] Token saved to " << filepath << std::endl;
    
    return true;
}

// ============================================================================
// Authentication Methods
// ============================================================================

bool OverteAuth::login(const std::string& username, const std::string& password, const std::string& metaverseUrl) {
    m_metaverseUrl = metaverseUrl;
    m_username = username;
    
    std::string tokenUrl = m_metaverseUrl;
    if (tokenUrl.back() == '/') tokenUrl.pop_back();
    
    // Overte uses /api/v1/oauth/token endpoint
    if (tokenUrl.find("/api/v1") == std::string::npos) {
        tokenUrl += "/api/v1";
    }
    tokenUrl += "/oauth/token";
    
    std::ostringstream postData;
    postData << "grant_type=password";
    postData << "&username=" << urlEncode(username);
    postData << "&password=" << urlEncode(password);
    postData << "&scope=owner";
    
    std::string response;
    if (!httpPost(tokenUrl, postData.str(), response)) {
        std::cerr << "[OverteAuth] Login failed: " << m_lastError << std::endl;
        return false;
    }
    
    if (!parseTokenResponse(response)) {
        std::cerr << "[OverteAuth] Failed to parse token: " << m_lastError << std::endl;
        return false;
    }
    
    std::cout << "[OverteAuth] Successfully authenticated as " << username << std::endl;
    return true;
}

bool OverteAuth::loginWithAuthCode(const std::string& authCode, const std::string& redirectUri) {
    std::string tokenUrl = m_metaverseUrl;
    if (tokenUrl.back() == '/') tokenUrl.pop_back();
    
    // Overte uses /api/v1/oauth/token endpoint
    if (tokenUrl.find("/api/v1") == std::string::npos) {
        tokenUrl += "/api/v1";
    }
    tokenUrl += "/oauth/token";
    
    std::ostringstream postData;
    postData << "grant_type=authorization_code";
    postData << "&code=" << urlEncode(authCode);
    postData << "&redirect_uri=" << urlEncode(redirectUri);
    postData << "&client_id=" << m_clientId;
    if (!m_clientSecret.empty()) {
        postData << "&client_secret=" << urlEncode(m_clientSecret);
    }
    
    std::string response;
    if (!httpPost(tokenUrl, postData.str(), response)) {
        std::cerr << "[OverteAuth] Token exchange failed: " << m_lastError << std::endl;
        return false;
    }
    
    if (!parseTokenResponse(response)) {
        std::cerr << "[OverteAuth] Failed to parse token: " << m_lastError << std::endl;
        return false;
    }
    
    std::cout << "[OverteAuth] Successfully exchanged authorization code for token" << std::endl;
    return true;
}

bool OverteAuth::refreshAccessToken() {
    if (m_refreshToken.empty()) {
        m_lastError = "No refresh token available";
        return false;
    }
    
    std::string tokenUrl = m_metaverseUrl;
    if (tokenUrl.back() == '/') tokenUrl.pop_back();
    
    // Overte uses /api/v1/oauth/token endpoint
    if (tokenUrl.find("/api/v1") == std::string::npos) {
        tokenUrl += "/api/v1";
    }
    tokenUrl += "/oauth/token";
    
    std::ostringstream postData;
    postData << "grant_type=refresh_token";
    postData << "&refresh_token=" << urlEncode(m_refreshToken);
    postData << "&scope=owner";
    
    std::string response;
    if (!httpPost(tokenUrl, postData.str(), response)) {
        std::cerr << "[OverteAuth] Token refresh failed: " << m_lastError << std::endl;
        // Clear tokens on refresh failure
        logout();
        return false;
    }
    
    if (!parseTokenResponse(response)) {
        std::cerr << "[OverteAuth] Failed to parse refreshed token: " << m_lastError << std::endl;
        logout();
        return false;
    }
    
    std::cout << "[OverteAuth] Successfully refreshed access token" << std::endl;
    return true;
}

void OverteAuth::logout() {
    m_accessToken.clear();
    m_refreshToken.clear();
    m_username.clear();
    m_tokenExpiresAt = 0;
    
    // Delete token file
    std::string filepath = getTokenFilePath();
    unlink(filepath.c_str());
    
    std::cout << "[OverteAuth] Logged out" << std::endl;
}

// ============================================================================
// OAuth Callback Server (for browser flow)
// ============================================================================

std::string OverteAuth::getCallbackURL() const {
    return "http://localhost:" + std::to_string(m_callbackPort) + "/callback";
}

bool OverteAuth::startCallbackServer() {
    if (m_callbackRunning) {
        return true; // Already running
    }
    
    m_callbackServerFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_callbackServerFd < 0) {
        m_lastError = "Failed to create socket";
        return false;
    }
    
    // Allow address reuse
    int opt = 1;
    setsockopt(m_callbackServerFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8765); // Try port 8765 first
    
    // Try to bind to port 8765, or let OS choose
    if (bind(m_callbackServerFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        // Let OS choose port
        addr.sin_port = 0;
        if (bind(m_callbackServerFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            m_lastError = "Failed to bind socket";
            close(m_callbackServerFd);
            m_callbackServerFd = -1;
            return false;
        }
    }
    
    // Get the actual port assigned
    socklen_t addrLen = sizeof(addr);
    if (getsockname(m_callbackServerFd, (struct sockaddr*)&addr, &addrLen) < 0) {
        m_lastError = "Failed to get socket name";
        close(m_callbackServerFd);
        m_callbackServerFd = -1;
        return false;
    }
    m_callbackPort = ntohs(addr.sin_port);
    
    if (listen(m_callbackServerFd, 5) < 0) {
        m_lastError = "Failed to listen on socket";
        close(m_callbackServerFd);
        m_callbackServerFd = -1;
        return false;
    }
    
    m_callbackRunning = true;
    m_callbackThread = std::make_unique<std::thread>(&OverteAuth::callbackServerThread, this);
    
    std::cout << "[OverteAuth] Callback server listening on port " << m_callbackPort << std::endl;
    return true;
}

void OverteAuth::stopCallbackServer() {
    if (!m_callbackRunning) {
        return;
    }
    
    m_callbackRunning = false;
    
    if (m_callbackServerFd >= 0) {
        close(m_callbackServerFd);
        m_callbackServerFd = -1;
    }
    
    if (m_callbackThread && m_callbackThread->joinable()) {
        m_callbackThread->join();
    }
    m_callbackThread.reset();
}

void OverteAuth::callbackServerThread() {
    std::cout << "[OverteAuth] Callback server thread started" << std::endl;
    
    while (m_callbackRunning) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_callbackServerFd, &readfds);
        
        struct timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(m_callbackServerFd + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (activity < 0 || !m_callbackRunning) {
            break;
        }
        
        if (activity == 0) {
            continue; // Timeout, check if still running
        }
        
        struct sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(m_callbackServerFd, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientFd < 0) {
            if (m_callbackRunning) {
                std::cerr << "[OverteAuth] Failed to accept connection" << std::endl;
            }
            continue;
        }
        
        handleCallbackRequest(clientFd);
        close(clientFd);
        
        // After handling one callback, we can stop
        m_callbackRunning = false;
    }
    
    std::cout << "[OverteAuth] Callback server thread stopped" << std::endl;
}

void OverteAuth::handleCallbackRequest(int clientFd) {
    char buffer[4096];
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        return;
    }
    
    buffer[bytesRead] = '\0';
    std::string request(buffer);
    
    // Parse HTTP GET request
    // Format: GET /callback?code=ABC&state=XYZ HTTP/1.1
    size_t getPos = request.find("GET /callback");
    if (getPos == std::string::npos) {
        const char* response = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(clientFd, response, strlen(response), 0);
        return;
    }
    
    // Extract code and state
    std::string code, state;
    size_t codePos = request.find("code=");
    if (codePos != std::string::npos) {
        size_t codeStart = codePos + 5;
        size_t codeEnd = request.find_first_of("& \r\n", codeStart);
        code = request.substr(codeStart, codeEnd - codeStart);
    }
    
    size_t statePos = request.find("state=");
    if (statePos != std::string::npos) {
        size_t stateStart = statePos + 6;
        size_t stateEnd = request.find_first_of("& \r\n", stateStart);
        state = request.substr(stateStart, stateEnd - stateStart);
    }
    
    // Verify state matches (CSRF protection)
    if (state != m_authState) {
        std::cerr << "[OverteAuth] State mismatch - possible CSRF attack!" << std::endl;
        const char* response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n"
            "<html><body><h1>Authentication Failed</h1><p>Invalid state parameter</p></body></html>";
        send(clientFd, response, strlen(response), 0);
        return;
    }
    
    if (code.empty()) {
        std::cerr << "[OverteAuth] No authorization code received" << std::endl;
        const char* response = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n"
            "<html><body><h1>Authentication Failed</h1><p>No authorization code received</p></body></html>";
        send(clientFd, response, strlen(response), 0);
        return;
    }
    
    m_receivedAuthCode = code;
    std::cout << "[OverteAuth] Received authorization code: " << code.substr(0, 10) << "..." << std::endl;
    
    // Send success response
    const char* response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
        "<html><body><h1>Authentication Successful!</h1>"
        "<p>You can now close this window and return to Starworld.</p>"
        "<script>window.close();</script></body></html>";
    send(clientFd, response, strlen(response), 0);
}

bool OverteAuth::loginWithBrowser(const std::string& metaverseUrl) {
    m_metaverseUrl = metaverseUrl;
    
    // Start callback server
    if (!startCallbackServer()) {
        std::cerr << "[OverteAuth] Failed to start callback server: " << m_lastError << std::endl;
        return false;
    }
    
    // Generate CSRF protection state
    m_authState = generateRandomState();
    m_receivedAuthCode.clear();
    
    // Construct authorization URL
    // Overte uses /api/v1/oauth/authorize endpoint
    std::string authUrl = m_metaverseUrl;
    if (authUrl.back() == '/') authUrl.pop_back();
    
    // Check if URL already has /api/v1 path, if not add it
    if (authUrl.find("/api/v1") == std::string::npos) {
        authUrl += "/api/v1";
    }
    authUrl += "/oauth/authorize?";
    authUrl += "response_type=code";
    authUrl += "&client_id=" + urlEncode(m_clientId);
    authUrl += "&redirect_uri=" + urlEncode(getCallbackURL());
    authUrl += "&scope=owner";
    authUrl += "&state=" + m_authState;
    
    std::cout << "[OverteAuth] ===================================" << std::endl;
    std::cout << "[OverteAuth] Opening browser for authentication" << std::endl;
    std::cout << "[OverteAuth] If browser doesn't open automatically, navigate to:" << std::endl;
    std::cout << "[OverteAuth] " << authUrl << std::endl;
    std::cout << "[OverteAuth] ===================================" << std::endl;
    
    if (!openBrowser(authUrl)) {
        std::cerr << "[OverteAuth] Failed to open browser" << std::endl;
        // Continue anyway - user can manually open URL
    }
    
    // Wait for callback (max 5 minutes)
    std::cout << "[OverteAuth] Waiting for authentication callback..." << std::endl;
    auto startTime = std::chrono::steady_clock::now();
    while (m_callbackRunning && m_receivedAuthCode.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        
        if (elapsed > 300) { // 5 minutes timeout
            std::cerr << "[OverteAuth] Authentication timeout" << std::endl;
            stopCallbackServer();
            return false;
        }
    }
    
    stopCallbackServer();
    
    if (m_receivedAuthCode.empty()) {
        m_lastError = "No authorization code received";
        return false;
    }
    
    // Exchange authorization code for access token
    std::cout << "[OverteAuth] Exchanging authorization code for access token..." << std::endl;
    return loginWithAuthCode(m_receivedAuthCode, getCallbackURL());
}
