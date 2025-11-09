#pragma once
#include <string>
#include <vector>

// Simple domain record discovered from metaverse API
struct DiscoveredDomain {
    std::string name;        // Friendly name if available
    std::string networkHost; // Hostname or IP
    int httpPort{40102};     // Control/HTTP port (defaults)
    int udpPort{40104};      // UDP domain port
};

// Fetch a list of candidate domains. Non-fatal if empty.
// Implementation attempts several known metaverse endpoints.
std::vector<DiscoveredDomain> discoverDomains(int maxDomains = 25);
