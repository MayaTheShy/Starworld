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

// Exposed for tests: parse a directory JSON payload into candidate domains.
std::vector<DiscoveredDomain> parseDomainsFromJson(const std::string& json);

// Probe a domain for TCP reachability on its httpPort (non-blocking, short timeout).
// Returns true if the domain appears reachable (TCP connect succeeds or is in progress).
bool probeDomain(const DiscoveredDomain& domain, int timeoutMs = 800);
