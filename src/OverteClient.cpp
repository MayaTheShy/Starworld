#include "OverteClient.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace std::chrono_literals;

// Generate a simple UUID-like string for session identification
static std::string generateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) ss << '-';
        ss << std::setw(2) << dis(gen);
    }
    return ss.str();
}

bool OverteClient::connect() {
    // Generate session UUID
    m_sessionUUID = generateUUID();
    std::cout << "[OverteClient] Session UUID: " << m_sessionUUID << std::endl;
    
    // Check for authentication credentials from environment
    const char* usernameEnv = std::getenv("OVERTE_USERNAME");
    const char* passwordEnv = std::getenv("OVERTE_PASSWORD");
    if (usernameEnv) m_username = usernameEnv;
    if (passwordEnv) m_password = passwordEnv;
    
    if (!m_username.empty()) {
        std::cout << "[OverteClient] Using authentication: username=" << m_username << std::endl;
    } else {
        std::cout << "[OverteClient] No authentication credentials provided (set OVERTE_USERNAME/OVERTE_PASSWORD)" << std::endl;
    }
    
    // Parse ws://host:port
    std::string url = m_domainUrl;
    if (url.empty()) url = "ws://127.0.0.1:40102";
    if (url.rfind("ws://", 0) == 0) url = url.substr(5);
    auto colon = url.find(':');
    m_host = colon == std::string::npos ? url : url.substr(0, colon);
    m_port = colon == std::string::npos ? 40102 : std::stoi(url.substr(colon + 1));
    
    // Check for environment override for UDP port (domain server UDP port)
    const char* portEnv = std::getenv("OVERTE_UDP_PORT");
    int udpPort = portEnv ? std::atoi(portEnv) : 40104; // Default to 40104 for Overte domain UDP
    
    std::cout << "[OverteClient] Connecting to domain at " << m_host 
              << " (HTTP:" << m_port << ", UDP:" << udpPort << ")" << std::endl;

    // Resolve host:port
    addrinfo hints{}; hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
    addrinfo* res = nullptr;
    int gai = ::getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(), &hints, &res);
    if (gai != 0) {
        std::cerr << "[OverteClient] getaddrinfo failed for " << m_host << ":" << m_port << " - " << gai_strerror(gai) << std::endl;
    } else {
        // Attempt TCP reachability for diagnostics
        int fd = -1; addrinfo* rp = res;
        for (; rp; rp = rp->ai_next) {
            fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd == -1) continue;
            ::fcntl(fd, F_SETFL, O_NONBLOCK);
            int c = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
            if (c == 0 || (c == -1 && errno == EINPROGRESS)) {
                std::cout << "[OverteClient] TCP reachable (non-blocking) to " << m_host << ":" << m_port << std::endl;
                ::close(fd); fd = -1; break;
            }
            ::close(fd); fd = -1;
        }
        ::freeaddrinfo(res);
        if (fd == -1) {
            // Not necessarily fatal; mixers are UDP. Continue with UDP.
        }
    }

    // Setup UDP to target (domain server UDP port)
    addrinfo uhints{}; uhints.ai_socktype = SOCK_DGRAM; uhints.ai_family = AF_UNSPEC;
    addrinfo* ures = nullptr;
    int ugai = ::getaddrinfo(m_host.c_str(), std::to_string(udpPort).c_str(), &uhints, &ures);
    if (ugai != 0) {
        std::cerr << "[OverteClient] UDP resolve failed: " << gai_strerror(ugai) << std::endl;
    } else {
        for (addrinfo* rp = ures; rp; rp = rp->ai_next) {
            m_udpFd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (m_udpFd == -1) continue;
            ::fcntl(m_udpFd, F_SETFL, O_NONBLOCK);
            std::memcpy(&m_udpAddr, rp->ai_addr, rp->ai_addrlen);
            m_udpAddrLen = rp->ai_addrlen;
            m_udpReady = true;
            std::cout << "[OverteClient] UDP socket ready for " << m_host << ":" << udpPort << std::endl;
            break;
        }
        ::freeaddrinfo(ures);
    }

    // Simulate successful connections to mixers.
    m_connected = connectAvatarMixer() && connectEntityServer() && connectAudioMixer();
    if (!m_connected) {
        std::cerr << "OverteClient: failed to connect one or more mixers" << std::endl;
        return false;
    }
    
    // Send domain connect request to initiate handshake
    // Start with domain list request - simpler packet
    std::cout << "[OverteClient] Initiating domain handshake..." << std::endl;
    sendDomainConnectRequest();
    sendDomainListRequest();

    m_useSimulation = (std::getenv("STARWORLD_SIMULATE") != nullptr);
    if (m_useSimulation) {
        // Seed a couple of demo entities.
        OverteEntity a{ m_nextEntityId++, "CubeA", glm::mat4(1.0f) };
        OverteEntity b{ m_nextEntityId++, "CubeB", glm::mat4(1.0f) };
        m_entities.emplace(a.id, a);
        m_entities.emplace(b.id, b);
        m_updateQueue.push_back(a.id);
        m_updateQueue.push_back(b.id);
        std::cout << "[OverteClient] Simulation mode enabled (STARWORLD_SIMULATE=1)" << std::endl;
    } else {
        std::cout << "[OverteClient] Waiting for entity packets from Overte server..." << std::endl;
        std::cout << "[OverteClient] Tip: Set STARWORLD_SIMULATE=1 to enable demo entities" << std::endl;
    }
    return true;
}

bool OverteClient::connectAvatarMixer() {
    // For now, consider UDP socket readiness as mixer connectivity proxy.
    m_avatarMixer = m_udpReady;
    return true;
}

bool OverteClient::connectEntityServer() {
    // Entity server connection will be established after DomainList reply
    // For now, create socket and bind to receive packets
    m_entityFd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_entityFd == -1) {
        std::cerr << "[OverteClient] Failed to create EntityServer socket: " << std::strerror(errno) << std::endl;
        return false;
    }
    
    // Make non-blocking
    ::fcntl(m_entityFd, F_SETFL, O_NONBLOCK);
    
    // Bind to ephemeral port (let OS choose) for receiving entity packets
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    bindAddr.sin_port = 0; // Let OS assign port
    
    if (::bind(m_entityFd, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == -1) {
        std::cerr << "[OverteClient] Failed to bind EntityServer socket: " << std::strerror(errno) << std::endl;
        ::close(m_entityFd);
        m_entityFd = -1;
        return false;
    }
    
    // Get the assigned port
    socklen_t addrLen = sizeof(bindAddr);
    if (::getsockname(m_entityFd, reinterpret_cast<sockaddr*>(&bindAddr), &addrLen) == 0) {
        std::cout << "[OverteClient] EntityServer socket bound to port " << ntohs(bindAddr.sin_port) << std::endl;
    }
    
    m_entityServer = true;
    return true;
}

bool OverteClient::connectAudioMixer() {
    // TODO: Connect AudioMixer for voice chat.
    m_audioMixer = true;
    return true;
}

void OverteClient::poll() {
    if (!m_connected) return;

    // Poll domain UDP socket for domain-level packets
    if (m_udpReady && m_udpFd != -1) {
        char buf[1500];
        sockaddr_storage from{}; socklen_t fromlen = sizeof(from);
        ssize_t r = ::recvfrom(m_udpFd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (r > 0) {
            std::cout << "[OverteClient] <<< Received domain packet (" << r << " bytes)" << std::endl;
            // Hex dump first 32 bytes for debugging
            std::cout << "[OverteClient] Hex: ";
            for (int i = 0; i < std::min(32, (int)r); ++i) {
                printf("%02x ", (unsigned char)buf[i]);
            }
            std::cout << std::endl;
            parseDomainPacket(buf, static_cast<size_t>(r));
        } else if (r < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
            // Only log errors that aren't "would block"
            static int errorCount = 0;
            if (++errorCount <= 3) {
                std::cerr << "[OverteClient] UDP recv error: " << strerror(errno) << std::endl;
            }
        }
        
        // Send periodic ping to domain to keep connection alive
        static auto lastPing = std::chrono::steady_clock::now();
        static auto lastDomainList = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPing).count() >= 1) {
            sendPing(m_udpFd, m_udpAddr, m_udpAddrLen);
            lastPing = now;
        }
        
        // Request domain list periodically if not connected
        if (!m_domainConnected && std::chrono::duration_cast<std::chrono::seconds>(now - lastDomainList).count() >= 3) {
            std::cout << "[OverteClient] Retrying domain handshake..." << std::endl;
            sendDomainConnectRequest();
            sendDomainListRequest();
            lastDomainList = now;
        }
    }

    // Parse entity server packets
    parseNetworkPackets();

    if (m_useSimulation) {
        // Simulate entity transforms changing slightly over time.
        static auto t0 = std::chrono::steady_clock::now();
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
        for (auto& [id, e] : m_entities) {
            const float r = 0.25f + 0.05f * static_cast<float>(id);
            const float x = std::cos(t * 0.5f + static_cast<float>(id)) * r;
            const float z = std::sin(t * 0.5f + static_cast<float>(id)) * r;
            e.transform = glm::translate(glm::mat4(1.0f), glm::vec3{x, 1.25f, z});
            m_updateQueue.push_back(id);
        }
    }
}

void OverteClient::parseNetworkPackets() {
    // Read from EntityServer socket
    if (m_entityServerReady && m_entityFd != -1) {
        char buf[1500];
        sockaddr_storage from{}; socklen_t fromlen = sizeof(from);
        ssize_t r = ::recvfrom(m_entityFd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (r > 0) {
            std::cout << "[OverteClient] EntityServer packet received (" << r << " bytes, type=0x" 
                      << std::hex << (int)(unsigned char)buf[0] << std::dec << ")" << std::endl;
            parseEntityPacket(buf, static_cast<size_t>(r));
        }
    }
}

void OverteClient::parseDomainPacket(const char* data, size_t len) {
    if (len < 1) return;
    
    unsigned char packetType = static_cast<unsigned char>(data[0]);
    
    // Domain-level packet types
    const unsigned char PACKET_TYPE_PING = 0x01;
    const unsigned char PACKET_TYPE_PING_REPLY = 0x02;
    const unsigned char PACKET_TYPE_DOMAIN_LIST = 0x03;
    const unsigned char PACKET_TYPE_DOMAIN_CONNECTION_DENIED = 0x06;
    const unsigned char PACKET_TYPE_DOMAIN_SERVER_REQUIRE_DT = 0x07;
    const unsigned char PACKET_TYPE_DOMAIN_CONNECT_ACK = 0x0A;
    
    std::cout << "[OverteClient] Domain packet type: 0x" << std::hex << (int)packetType << std::dec << std::endl;
    
    switch (packetType) {
        case PACKET_TYPE_DOMAIN_LIST:
            handleDomainListReply(data + 1, len - 1);
            break;
            
        case PACKET_TYPE_DOMAIN_CONNECTION_DENIED:
            handleDomainConnectionDenied(data + 1, len - 1);
            break;
            
        case PACKET_TYPE_DOMAIN_CONNECT_ACK:
            std::cout << "[OverteClient] Domain connection acknowledged!" << std::endl;
            m_domainConnected = true;
            // Request domain list after successful connection
            sendDomainListRequest();
            break;
            
        case PACKET_TYPE_PING_REPLY:
            // Keep-alive ping reply
            break;
            
        default:
            std::cout << "[OverteClient] Unknown domain packet type: 0x" << std::hex << (int)packetType << std::dec << std::endl;
            break;
    }
}

void OverteClient::parseEntityPacket(const char* data, size_t len) {
    // Overte packet structure (simplified):
    // - Byte 0: PacketType
    // - Following bytes: payload (varies by type)
    
    if (len < 1) return;
    
    unsigned char packetType = static_cast<unsigned char>(data[0]);
    
    // Entity packet types
    const unsigned char PACKET_TYPE_ENTITY_ADD = 0x10;
    const unsigned char PACKET_TYPE_ENTITY_EDIT = 0x11;
    const unsigned char PACKET_TYPE_ENTITY_ERASE = 0x12;
    const unsigned char PACKET_TYPE_ENTITY_QUERY = 0x15;
    const unsigned char PACKET_TYPE_OCTREE_STATS = 0x16;
    const unsigned char PACKET_TYPE_ENTITY_DATA = 0x41; // Bulk entity data response
    
    switch (packetType) {
        case PACKET_TYPE_ENTITY_DATA:
        case PACKET_TYPE_ENTITY_ADD: {
            // EntityAdd/EntityData packet structure (simplified):
            // u64 entityID, string name, vec3 position, quat rotation, vec3 dimensions, ...
            if (len < 9) break; // need at least 1+8 bytes
            
            std::uint64_t entityId;
            std::memcpy(&entityId, data + 1, 8);
            
            // Parse name (null-terminated string after ID)
            size_t offset = 9;
            std::string name;
            while (offset < len && data[offset] != '\0') {
                name += data[offset++];
            }
            if (name.empty()) name = "Entity_" + std::to_string(entityId);
            
            // TODO: Parse full entity properties (position, rotation, dimensions)
            // For now, create entity with a visible position spread out in front of user
            // Position entities in a grid pattern for visibility
            float spacing = 0.5f;
            int index = static_cast<int>(entityId % 10);
            float x = (index % 3) * spacing - spacing;  // -0.5, 0, 0.5
            float y = 1.5f;  // Eye level
            float z = -2.0f - (index / 3) * spacing;  // Start 2m in front, spread back
            
            glm::vec3 position(x, y, z);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
            
            OverteEntity entity{entityId, name, transform};
            m_entities[entityId] = entity;
            m_updateQueue.push_back(entityId);
            
            std::cout << "[OverteClient] Entity added: " << name << " (id=" << entityId 
                      << ") at pos(" << x << ", " << y << ", " << z << ")" << std::endl;
            break;
        }
        
        case PACKET_TYPE_ENTITY_EDIT: {
            // EntityEdit packet: u64 entityID, property flags, property data...
            if (len < 9) break;
            
            std::uint64_t entityId;
            std::memcpy(&entityId, data + 1, 8);
            
            auto it = m_entities.find(entityId);
            if (it != m_entities.end()) {
                // TODO: parse property flags and update transform
                // For now, mark as updated
                m_updateQueue.push_back(entityId);
                std::cout << "[OverteClient] Entity edited: id=" << entityId << std::endl;
            }
            break;
        }
        
        case PACKET_TYPE_ENTITY_ERASE: {
            // EntityErase packet: u64 entityID
            if (len < 9) break;
            
            std::uint64_t entityId;
            std::memcpy(&entityId, data + 1, 8);
            
            auto it = m_entities.find(entityId);
            if (it != m_entities.end()) {
                m_entities.erase(it);
                m_deleteQueue.push_back(entityId);
                std::cout << "[OverteClient] Entity erased: id=" << entityId << std::endl;
            }
            break;
        }
        
        case PACKET_TYPE_OCTREE_STATS:
            std::cout << "[OverteClient] Received octree stats" << std::endl;
            break;
            
        default:
            std::cout << "[OverteClient] Unknown entity packet type: 0x" << std::hex << (int)packetType << std::dec << std::endl;
            break;
    }
}

void OverteClient::handleDomainListReply(const char* data, size_t len) {
    // DomainList packet contains mixer endpoints
    // Format: [NumNodes:u8] followed by sequence of:
    // [NodeType:u8][UUID:16bytes][PublicSocket:sockaddr][LocalSocket:sockaddr]
    std::cout << "[OverteClient] DomainList reply received (" << len << " bytes)" << std::endl;
    
    if (len < 1) return;
    
    unsigned char numNodes = static_cast<unsigned char>(data[0]);
    std::cout << "[OverteClient] Number of assignment clients: " << (int)numNodes << std::endl;
    
    size_t offset = 1;
    
    for (int i = 0; i < numNodes && offset < len; ++i) {
        // Read NodeType
        if (offset + 1 > len) break;
        unsigned char nodeType = static_cast<unsigned char>(data[offset++]);
        
        // Skip UUID (16 bytes)
        if (offset + 16 > len) break;
        offset += 16;
        
        // Read public socket address
        if (offset + sizeof(sockaddr_in) > len) break;
        
        sockaddr_in publicAddr;
        std::memcpy(&publicAddr, data + offset, sizeof(sockaddr_in));
        offset += sizeof(sockaddr_in);
        
        // Skip local socket (same size)
        if (offset + sizeof(sockaddr_in) > len) break;
        offset += sizeof(sockaddr_in);
        
        // NodeType values from Overte:
        // 0 = DomainServer, 1 = EntityServer, 2 = Agent, 3 = AudioMixer, 
        // 4 = AvatarMixer, 5 = AssetServer, 6 = MessagesMixer, 7 = EntityScriptServer
        const unsigned char NODE_TYPE_ENTITY_SERVER = 1;
        const unsigned char NODE_TYPE_AVATAR_MIXER = 4;
        const unsigned char NODE_TYPE_AUDIO_MIXER = 3;
        
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &publicAddr.sin_addr, addrStr, sizeof(addrStr));
        int port = ntohs(publicAddr.sin_port);
        
        const char* nodeTypeName = "Unknown";
        switch (nodeType) {
            case 0: nodeTypeName = "DomainServer"; break;
            case NODE_TYPE_ENTITY_SERVER: nodeTypeName = "EntityServer"; break;
            case 2: nodeTypeName = "Agent"; break;
            case NODE_TYPE_AUDIO_MIXER: nodeTypeName = "AudioMixer"; break;
            case NODE_TYPE_AVATAR_MIXER: nodeTypeName = "AvatarMixer"; break;
            case 5: nodeTypeName = "AssetServer"; break;
            case 6: nodeTypeName = "MessagesMixer"; break;
            case 7: nodeTypeName = "EntityScriptServer"; break;
        }
        
        std::cout << "[OverteClient] Assignment: " << nodeTypeName 
                  << " at " << addrStr << ":" << port << std::endl;
        
        if (nodeType == NODE_TYPE_ENTITY_SERVER) {
            // Update EntityServer connection to use discovered address
            std::cout << "[OverteClient] Connecting to EntityServer at " << addrStr << ":" << port << std::endl;
            
            // Update target address for EntityServer
            sockaddr_in* entityAddr = reinterpret_cast<sockaddr_in*>(&m_entityAddr);
            entityAddr->sin_family = AF_INET;
            entityAddr->sin_port = publicAddr.sin_port;
            entityAddr->sin_addr = publicAddr.sin_addr;
            m_entityAddrLen = sizeof(sockaddr_in);
            
            m_entityServerReady = true;
            
            // Send EntityQuery to request all entities
            sendEntityQuery();
        }
    }
}

void OverteClient::handleDomainConnectionDenied(const char* data, size_t len) {
    std::cerr << "[OverteClient] Domain connection DENIED!" << std::endl;
    
    // Parse reason if available
    if (len > 0) {
        std::string reason(data, len);
        std::cerr << "[OverteClient] Reason: " << reason << std::endl;
    }
    
    m_domainConnected = false;
}

void OverteClient::sendDomainConnectRequest() {
    if (!m_udpReady || m_udpFd == -1) return;
    
    // DomainConnectRequest packet (PacketType 0x04)
    const unsigned char PACKET_TYPE_DOMAIN_CONNECT_REQUEST = 0x04;
    
    // Build packet with authentication
    // Format: [PacketType][ProtocolVersion:string][HardwareAddress:string][MachineFingerprint:string][Username:string][Password:string]
    std::string protocolVersion = "Starworld-0.1";
    std::string hardwareAddress = m_sessionUUID;
    std::string machineFingerprint = m_sessionUUID; // Use session UUID as fingerprint
    
    std::vector<char> packet;
    packet.push_back(static_cast<char>(PACKET_TYPE_DOMAIN_CONNECT_REQUEST));
    
    // Helper lambda to add length-prefixed string
    auto addString = [&packet](const std::string& str) {
        if (str.size() > 255) {
            packet.push_back(static_cast<char>(255));
            packet.insert(packet.end(), str.begin(), str.begin() + 255);
        } else {
            packet.push_back(static_cast<char>(str.size()));
            packet.insert(packet.end(), str.begin(), str.end());
        }
    };
    
    // Add protocol version
    addString(protocolVersion);
    
    // Add hardware address (session UUID)
    addString(hardwareAddress);
    
    // Add machine fingerprint
    addString(machineFingerprint);
    
    // Add username (empty if not authenticated)
    addString(m_username);
    
    // Add password/token (empty if not authenticated)
    addString(m_password);
    
    ssize_t s = ::sendto(m_udpFd, packet.data(), packet.size(), 0, 
                         reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
    if (s > 0) {
        std::cout << "[OverteClient] Domain connect request sent (" << s << " bytes)" << std::endl;
        std::cout << "[OverteClient]   Protocol: " << protocolVersion << std::endl;
        std::cout << "[OverteClient]   Session: " << hardwareAddress << std::endl;
        if (!m_username.empty()) {
            std::cout << "[OverteClient]   Username: " << m_username << std::endl;
        }
        // Hex dump first 32 bytes
        std::cout << "[OverteClient] >>> Hex: ";
        for (size_t i = 0; i < std::min(size_t(32), packet.size()); ++i) {
            printf("%02x ", (unsigned char)packet[i]);
        }
        std::cout << std::endl;
    } else {
        std::cerr << "[OverteClient] Failed to send domain connect request: " << strerror(errno) << std::endl;
    }
}

void OverteClient::sendDomainListRequest() {
    // Send DomainList request packet (PacketType 0x02)
    if (!m_udpReady || m_udpFd == -1) return;
    
    const unsigned char PACKET_TYPE_DOMAIN_LIST_REQUEST = 0x02;
    char packet[1] = { static_cast<char>(PACKET_TYPE_DOMAIN_LIST_REQUEST) };
    
    ssize_t s = ::sendto(m_udpFd, packet, sizeof(packet), 0, 
                         reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
    if (s > 0) {
        std::cout << "[OverteClient] DomainList request sent" << std::endl;
    } else {
        std::cerr << "[OverteClient] Failed to send domain list request: " << strerror(errno) << std::endl;
    }
}

void OverteClient::sendPing(int fd, const sockaddr_storage& addr, socklen_t addrLen) {
    const unsigned char PACKET_TYPE_PING = 0x01;
    
    // Ping packet: [PacketType][Timestamp:u64][PingType:u8]
    char packet[1 + 8 + 1];
    packet[0] = static_cast<char>(PACKET_TYPE_PING);
    
    // Add timestamp (microseconds since epoch)
    auto now = std::chrono::system_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    std::memcpy(packet + 1, &micros, 8);
    
    // Ping type (0 = local, 1 = public)
    packet[9] = 0;
    
    ssize_t s = ::sendto(fd, packet, sizeof(packet), 0, 
                         reinterpret_cast<const sockaddr*>(&addr), addrLen);
    if (s < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
        std::cerr << "[OverteClient] Ping send failed: " << strerror(errno) << std::endl;
    }
}

void OverteClient::sendEntityQuery() {
    if (m_entityFd < 0 || !m_entityServerReady) return;
    
    const unsigned char PACKET_TYPE_ENTITY_QUERY = 0x15;
    
    // EntityQuery packet structure (simplified):
    // [PacketType:u8][ConicalViews:bool][CameraFrustum if ConicalViews=true]
    // For simplicity, send with ConicalViews=false to request all entities
    
    std::vector<char> packet;
    packet.push_back(static_cast<char>(PACKET_TYPE_ENTITY_QUERY));
    packet.push_back(0); // ConicalViews = false
    
    // With ConicalViews=false, we're requesting all entities
    // Additional octree query parameters can be added here
    
    ssize_t sent = sendto(m_entityFd, packet.data(), packet.size(), 0, 
                         reinterpret_cast<const sockaddr*>(&m_entityAddr), m_entityAddrLen);
    
    if (sent > 0) {
        std::cout << "[OverteClient] Sent EntityQuery to EntityServer" << std::endl;
    } else {
        std::cerr << "[OverteClient] Failed to send EntityQuery: " << strerror(errno) << std::endl;
    }
}

void OverteClient::sendMovementInput(const glm::vec3& linearVelocity) {
    (void)linearVelocity; // TODO: send to avatar mixer
}

std::vector<OverteEntity> OverteClient::consumeUpdatedEntities() {
    std::vector<OverteEntity> out;
    out.reserve(m_updateQueue.size());
    for (auto id : m_updateQueue) {
        auto it = m_entities.find(id);
        if (it != m_entities.end()) out.push_back(it->second);
    }
    m_updateQueue.clear();
    return out;
}

std::vector<std::uint64_t> OverteClient::consumeDeletedEntities() {
    std::vector<std::uint64_t> out;
    out.swap(m_deleteQueue); // efficient clear
    return out;
}
