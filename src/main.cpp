// main.cpp
#include "StardustBridge.hpp"
#include "OverteClient.hpp"
#include "SceneSync.Hpp"
#include "InputHandler.hpp"
#include "DomainDiscovery.hpp"
#include "OverteAuth.hpp"

#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    // Simple CLI: --socket=/path/to.sock or --abstract=name
    std::string socketOverride;
    bool useAuth = false;
    std::string authUsername, authPassword;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        const std::string so = "--socket=";
        const std::string ab = "--abstract=";
        const std::string authFlag = "--auth";
        const std::string userFlag = "--username=";
        const std::string passFlag = "--password=";
        
        if (arg.rfind(so, 0) == 0) socketOverride = arg.substr(so.size());
        else if (arg.rfind(ab, 0) == 0) socketOverride = '@' + arg.substr(ab.size());
        else if (arg == authFlag) useAuth = true;
        else if (arg.rfind(userFlag, 0) == 0) authUsername = arg.substr(userFlag.size());
        else if (arg.rfind(passFlag, 0) == 0) authPassword = arg.substr(passFlag.size());
    }
    
    // Handle OAuth authentication if requested
    OverteAuth auth;
    if (useAuth) {
        std::string metaverseUrl = "https://mv.overte.org";
        if (const char* env = std::getenv("OVERTE_METAVERSE")) {
            metaverseUrl = env;
        }
        
        std::cout << "[Auth] ===================================" << std::endl;
        std::cout << "[Auth] Overte OAuth Authentication" << std::endl;
        std::cout << "[Auth] Metaverse: " << metaverseUrl << std::endl;
        std::cout << "[Auth] ===================================" << std::endl;
        
        bool authenticated = false;
        
        // Try loading saved token first
        if (auth.loadTokenFromFile()) {
            if (auth.isAuthenticated()) {
                std::cout << "[Auth] Using saved token for " << auth.getUsername() << std::endl;
                authenticated = true;
            }
        }
        
        if (!authenticated) {
            if (!authUsername.empty() && !authPassword.empty()) {
                // Use provided credentials (less secure, not recommended)
                std::cout << "[Auth] Authenticating with username/password..." << std::endl;
                authenticated = auth.login(authUsername, authPassword, metaverseUrl);
            } else {
                // Use browser OAuth flow (recommended)
                std::cout << "[Auth] Starting browser-based authentication..." << std::endl;
                authenticated = auth.loginWithBrowser(metaverseUrl);
            }
        }
        
        if (!authenticated) {
            std::cerr << "[Auth] Authentication failed: " << auth.getLastError() << std::endl;
            std::cerr << "[Auth] Continuing in anonymous mode..." << std::endl;
        } else {
            std::cout << "[Auth] ✓ Successfully authenticated!" << std::endl;
            std::cout << "[Auth] Username: " << auth.getUsername() << std::endl;
        }
    }
    
    StardustBridge stardust;
    if (!stardust.connect(socketOverride)) {
        std::cerr << "Failed to connect to StardustXR compositor.\n";
        return 1;
    }
    
    std::cout << "[main] StardustXR connected, continuing to Overte setup..." << std::endl;
    std::cout.flush();  // Force flush to ensure message appears

    // Overte localhost default assumption (can override via OVERTE_URL env or --overte=ws://host:port)
    std::string overteUrl = "ws://127.0.0.1:40102";
    bool useDiscovery = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        const std::string ov = "--overte=";
        const std::string disc = "--discover";
        if (arg.rfind(ov, 0) == 0) overteUrl = arg.substr(ov.size());
        else if (arg == disc) useDiscovery = true;
    }
    if (const char* envOv = std::getenv("OVERTE_URL")) {
        overteUrl = envOv;
    }
    if (const char* envDisc = std::getenv("OVERTE_DISCOVER")) {
        if (std::string(envDisc) == "1" || std::string(envDisc) == "true") useDiscovery = true;
    }

    if (useDiscovery) {
        std::cout << "[Discovery] Querying metaverse directories for public domains..." << std::endl;
        auto domains = discoverDomains(25);
        if (domains.empty()) {
            std::cerr << "[Discovery] ERROR: No public domains found via metaverse directories." << std::endl;
            std::cerr << "[Discovery] The metaverse directory services may be unreachable." << std::endl;
            std::cerr << "[Discovery] To connect to a specific server, use:" << std::endl;
            std::cerr << "[Discovery]   ./build/starworld ws://SERVER_ADDRESS:40102" << std::endl;
            std::cerr << "[Discovery] Or set OVERTE_URL environment variable." << std::endl;
            return 1;
        } else {
            std::cout << "[Discovery] Found " << domains.size() << " candidate domain(s):" << std::endl;
            
            // Limit display to first 10
            int displayLimit = std::min(10, (int)domains.size());
            for (int idx = 0; idx < displayLimit; ++idx) {
                const auto& d = domains[idx];
                std::cout << "  [" << idx << "] "
                          << (d.name.empty() ? d.networkHost : d.name)
                          << " -> ws://" << d.networkHost << ":" << d.httpPort
                          << " (udp:" << d.udpPort << ")" << std::endl;
            }
            if ((int)domains.size() > displayLimit) {
                std::cout << "  ... and " << (domains.size() - displayLimit) << " more domains" << std::endl;
            }
            
            // Probe for reachability unless disabled (disabled by default for large lists)
            bool probeEnabled = false; // Default: disabled for performance
            if (const char* envProbe = std::getenv("OVERTE_DISCOVER_PROBE")) {
                if (std::string(envProbe) == "1" || std::string(envProbe) == "true") probeEnabled = true;
            }
            
            int choice = -1;
            if (probeEnabled) {
                std::cout << "[Discovery] Probing domains for reachability (limit 20)..." << std::endl;
                int probeLimit = std::min(20, (int)domains.size());
                for (int i = 0; i < probeLimit; ++i) {
                    std::cout << "[Discovery] Probing [" << i << "] " << domains[i].networkHost << ":" << domains[i].httpPort << "... " << std::flush;
                    if (probeDomain(domains[i])) {
                        std::cout << "REACHABLE" << std::endl;
                        choice = static_cast<int>(i);
                        break;
                    } else {
                        std::cout << "unreachable" << std::endl;
                    }
                }
                if (choice < 0) {
                    std::cout << "[Discovery] No reachable domains found in first " << probeLimit << "; using first candidate." << std::endl;
                    choice = 0;
                }
            } else {
                std::cout << "[Discovery] Probing disabled; selecting first candidate." << std::endl;
                std::cout << "[Discovery] Set OVERTE_DISCOVER_PROBE=1 to enable reachability testing." << std::endl;
                choice = 0;
            }
            
            // Allow override index via env (supersedes probe)
            if (const char* envIdx = std::getenv("OVERTE_DISCOVER_INDEX")) {
                try { 
                    int manualChoice = std::stoi(envIdx);
                    if (manualChoice >= 0 && manualChoice < (int)domains.size()) {
                        choice = manualChoice;
                        std::cout << "[Discovery] Manual override: selecting index " << choice << std::endl;
                    }
                } catch (...) {}
            }
            
            if (choice < 0 || choice >= (int)domains.size()) choice = 0;
            
            const auto& pick = domains[choice];
            overteUrl = std::string("ws://") + pick.networkHost + ":" + std::to_string(pick.httpPort);
            // Pass UDP override via env for this process lifetime
            setenv("OVERTE_UDP_PORT", std::to_string(pick.udpPort).c_str(), 1);
            std::cout << "[Discovery] Selected: " << overteUrl << std::endl;
        }
    }
    
    std::cout << "[main] Connecting to Overte domain: " << overteUrl << std::endl;
    OverteClient overte(overteUrl);
    
    // Pass authentication to OverteClient if we authenticated with metaverse
    if (useAuth && auth.isAuthenticated()) {
        overte.setAuth(&auth);
    }
    
    // Overte is optional; warn if unreachable but continue in offline mode.
    if (!overte.connect()) {
        std::cerr << "[main] Overte domain unreachable; running in offline mode." << std::endl;
        std::cerr << "[main] Tip: Use --overte=host:port to specify a domain, or set STARWORLD_SIMULATE=1" << std::endl;
    } else {
        std::cout << "[main] Overte connection established" << std::endl;
    }

    InputHandler input(stardust, overte);

    // Main loop
    while (stardust.running()) {
    overte.poll();
        stardust.poll();

        // Sync avatars/entities
        SceneSync::update(stardust, overte);

        // Simple input mapping
        input.update(1.0f / 90.0f);

        // Small sleep to avoid busy-spin in the stub
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }

    return 0;
}

