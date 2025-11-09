#include "DomainDiscovery.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>

#include <curl/curl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

struct Buffer { std::string data; };


size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* b = reinterpret_cast<Buffer*>(userdata);
    b->data.append(ptr, size * nmemb);
    return size * nmemb;
}

std::optional<std::string> httpGet(const std::string& url, long timeoutMs = 3000) {
    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;
    Buffer buf;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    // Optional auth header from env if needed
    struct curl_slist* headers = nullptr;
    if (const char* token = std::getenv("METAVERSE_TOKEN")) {
        std::string h = std::string("Authorization: Bearer ") + token;
        headers = curl_slist_append(headers, h.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    CURLcode rc = curl_easy_perform(curl);
    long code = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK || code < 200 || code >= 300) return std::nullopt;
    return buf.data;
}

// Very small JSON helpers (avoid adding a full JSON lib):
// Extract values for keys we care about with a permissive search.
std::vector<std::string> findAllStrings(const std::string& json, const std::string& key) {
    std::vector<std::string> out;
    std::string needle = '"' + key + '"';
    size_t pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        size_t colon = json.find(':', pos + needle.size()); if (colon == std::string::npos) break;
        size_t quote1 = json.find('"', colon + 1); if (quote1 == std::string::npos) break;
        if (json[quote1-1] == '\\') { pos = quote1 + 1; continue; }
        size_t quote2 = json.find('"', quote1 + 1); if (quote2 == std::string::npos) break;
        if (quote2 > quote1) {
            out.emplace_back(json.substr(quote1 + 1, quote2 - quote1 - 1));
        }
        pos = quote2 + 1;
    }
    return out;
}

std::vector<int> findAllInts(const std::string& json, const std::string& key) {
    std::vector<int> out;
    std::string needle = '"' + key + '"';
    size_t pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        size_t colon = json.find(':', pos + needle.size()); if (colon == std::string::npos) break;
        size_t start = json.find_first_of("-0123456789", colon + 1); if (start == std::string::npos) break;
        size_t end = json.find_first_not_of("0123456789", start + ((json[start] == '-') ? 1 : 0));
        std::string num = json.substr(start, end - start);
        try { out.emplace_back(std::stoi(num)); } catch (...) {}
        pos = end;
    }
    return out;
}

} // anonymous namespace

// Heuristic: map fields from common metaverse JSONs
// Vircadia/Overte often expose entries with fields like name, network_address, domain, ice_server_address, port, etc.
std::vector<DiscoveredDomain> parseDomains(const std::string& json) {
    std::vector<DiscoveredDomain> out;
    auto names    = findAllStrings(json, "name");
    auto hostsA   = findAllStrings(json, "network_address");
    auto hostsB   = findAllStrings(json, "ice_server_address");
    auto hostsC   = findAllStrings(json, "domain");
    auto hostsD   = findAllStrings(json, "address"); // alternative key
    auto httpPorts  = findAllInts(json, "http_port");
    auto httpPorts2 = findAllInts(json, "domain_http_port");
    auto udpPorts   = findAllInts(json, "udp_port");
    auto udpPorts2  = findAllInts(json, "domain_udp_port");

    // Gather candidates from each host list
    auto addHostList = [&](const std::vector<std::string>& hosts) {
        for (size_t i = 0; i < hosts.size(); ++i) {
            DiscoveredDomain d;
            d.name = (i < names.size()) ? names[i] : std::string();
            d.networkHost = hosts[i];
            int hp = (i < httpPorts.size() && httpPorts[i] > 0) ? httpPorts[i]
                   : (i < httpPorts2.size() && httpPorts2[i] > 0) ? httpPorts2[i] : 40102;
            int up = (i < udpPorts.size() && udpPorts[i] > 0) ? udpPorts[i]
                   : (i < udpPorts2.size() && udpPorts2[i] > 0) ? udpPorts2[i] : 40104;
            d.httpPort = hp; d.udpPort = up;
            out.emplace_back(std::move(d));
        }
    };
    addHostList(hostsA);
    addHostList(hostsB);
    addHostList(hostsC);
    addHostList(hostsD);

    // Dedup by host:port
    std::vector<DiscoveredDomain> dedup;
    for (auto& d : out) {
        bool exists = false;
        for (auto& x : dedup) {
            if (x.networkHost == d.networkHost && x.httpPort == d.httpPort && x.udpPort == d.udpPort) { exists = true; break; }
        }
        if (!exists && !d.networkHost.empty()) dedup.emplace_back(std::move(d));
    }
    return dedup;
}

std::vector<DiscoveredDomain> discoverDomains(int maxDomains) {
    std::vector<DiscoveredDomain> result;
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Check if verbose logging is enabled
    bool verbose = (std::getenv("OVERTE_DISCOVER_VERBOSE") != nullptr);

    // Allow override of endpoint via env
    std::vector<std::string> endpoints;
    if (const char* custom = std::getenv("METAVERSE_DISCOVERY_URL")) {
        endpoints.emplace_back(custom);
    }
    // Build fuller list of endpoints using base + path permutations
    std::vector<std::string> bases;
    if (const char* base = std::getenv("OVERTE_METAVERSE_BASE")) bases.emplace_back(base);
    bases.emplace_back("https://metaverse.vircadia.com");
    bases.emplace_back("https://metaverse.overte.org");
    bases.emplace_back("https://metaverse.overte.dev");
    bases.emplace_back("https://overte.org");
    const char* paths[] = {"/api/domains?status=online","/api/domains","/api/v1/domains?status=online","/api/v1/domains"};
    for (auto& b : bases) for (auto p : paths) endpoints.emplace_back(b + std::string(p));

    if (verbose) {
        std::cout << "[Discovery] Trying " << endpoints.size() << " directory endpoints..." << std::endl;
    }

    for (const auto& url : endpoints) {
        if (verbose) {
            std::cout << "[Discovery] Querying: " << url << std::endl;
        }
        auto body = httpGet(url);
        if (!body) {
            if (verbose) {
                std::cout << "[Discovery]   -> Failed (timeout or HTTP error)" << std::endl;
            }
            continue;
        }
        if (verbose) {
            std::cout << "[Discovery]   -> Got " << body->size() << " bytes" << std::endl;
        }
        auto list = parseDomains(*body);
        if (verbose) {
            std::cout << "[Discovery]   -> Parsed " << list.size() << " domains" << std::endl;
        }
        for (auto& d : list) {
            result.emplace_back(std::move(d));
            if ((int)result.size() >= maxDomains) break;
        }
        if ((int)result.size() >= maxDomains) break;
    }

    curl_global_cleanup();
    return result;
}

std::vector<DiscoveredDomain> parseDomainsFromJson(const std::string& json) {
    return parseDomains(json);
}

// Simple TCP reachability probe (non-blocking connect + select)
bool probeDomain(const DiscoveredDomain& domain, int timeoutMs) {
    addrinfo hints{}; 
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_family = AF_UNSPEC;
    addrinfo* res = nullptr;
    
    if (getaddrinfo(domain.networkHost.c_str(), std::to_string(domain.httpPort).c_str(), &hints, &res) != 0) {
        return false;
    }
    
    bool reachable = false;
    for (addrinfo* rp = res; rp; rp = rp->ai_next) {
        int fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        
        // Set non-blocking
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        
        int c = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (c == 0) {
            // Immediate success (rare for TCP)
            reachable = true;
            ::close(fd);
            break;
        }
        
        if (errno == EINPROGRESS) {
            // Wait for connect to complete or timeout
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(fd, &writefds);
            
            struct timeval tv;
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            
            int sel = select(fd + 1, nullptr, &writefds, nullptr, &tv);
            if (sel > 0 && FD_ISSET(fd, &writefds)) {
                // Check if connection succeeded
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                    reachable = true;
                    ::close(fd);
                    break;
                }
            }
        }
        
        ::close(fd);
    }
    
    if (res) freeaddrinfo(res);
    return reachable;
}
