// OverteClient.cpp
#include "OverteClient.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace std::chrono_literals;

bool OverteClient::connect() {
    // Parse ws://host:port
    std::string url = m_domainUrl;
    if (url.empty()) url = "ws://127.0.0.1:40102";
    if (url.rfind("ws://", 0) == 0) url = url.substr(5);
    auto colon = url.find(':');
    m_host = colon == std::string::npos ? url : url.substr(0, colon);
    m_port = colon == std::string::npos ? 40102 : std::stoi(url.substr(colon + 1));

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

    // Setup UDP to target (avatar mixer guess: same port by default)
    addrinfo uhints{}; uhints.ai_socktype = SOCK_DGRAM; uhints.ai_family = AF_UNSPEC;
    addrinfo* ures = nullptr;
    int ugai = ::getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(), &uhints, &ures);
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
            std::cout << "[OverteClient] UDP socket ready for " << m_host << ":" << m_port << std::endl;
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
    // Send DomainList request to discover EntityServer endpoint
    sendDomainListRequest();
    
    // Create UDP socket for EntityServer and BIND it to receive packets
    m_entityFd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_entityFd == -1) {
        std::cerr << "[OverteClient] Failed to create EntityServer socket: " << std::strerror(errno) << std::endl;
        return false;
    }
    
    // Make non-blocking
    ::fcntl(m_entityFd, F_SETFL, O_NONBLOCK);
    
    // Bind to port 40103 to receive entity packets
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    bindAddr.sin_port = htons(m_port + 1);   // 40103
    
    if (::bind(m_entityFd, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == -1) {
        std::cerr << "[OverteClient] Failed to bind EntityServer socket to port " << (m_port + 1) 
                  << ": " << std::strerror(errno) << std::endl;
        ::close(m_entityFd);
        m_entityFd = -1;
        return false;
    }
    
    // Store the target address for sending (if needed)
    m_entityAddr = {};
    sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&m_entityAddr);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(m_port + 1);
    ::inet_pton(AF_INET, m_host.c_str(), &addr->sin_addr);
    m_entityAddrLen = sizeof(sockaddr_in);
    
    m_entityServerReady = true;
    std::cout << "[OverteClient] EntityServer socket bound and listening on port " << (m_port + 1) << std::endl;
    
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

    // Try a lightweight UDP ping if ready (placeholder for avatar mixer handshake)
    if (m_udpReady && m_udpFd != -1) {
        const char ping[4] = {'P','I','N','G'};
        ssize_t s = ::sendto(m_udpFd, ping, sizeof(ping), 0, reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
        if (s == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "[OverteClient] UDP send failed: " << std::strerror(errno) << std::endl;
        }
        char buf[1500];
        sockaddr_storage from{}; socklen_t fromlen = sizeof(from);
        ssize_t r = ::recvfrom(m_udpFd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (r > 0) {
            // Parse as potential domain/avatar packets
            std::cout << "[OverteClient] Domain UDP packet received (" << r << " bytes, type=0x" 
                      << std::hex << (int)(unsigned char)buf[0] << std::dec << ")" << std::endl;
            parseEntityPacket(buf, static_cast<size_t>(r));
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

void OverteClient::parseEntityPacket(const char* data, size_t len) {
    // Overte packet structure (simplified):
    // - Byte 0: PacketType
    // - Following bytes: payload (varies by type)
    
    if (len < 1) return;
    
    unsigned char packetType = static_cast<unsigned char>(data[0]);
    
    // Overte PacketType enum values (reference from protocol documentation)
    // Reference: https://github.com/overte-org/overte/blob/master/libraries/networking/src/udt/PacketHeaders.h
    const unsigned char PACKET_TYPE_DOMAIN_LIST = 0x03;
    const unsigned char PACKET_TYPE_DOMAIN_CONNECTION_DENIED = 0x06;
    const unsigned char PACKET_TYPE_PING = 0x01;
    const unsigned char PACKET_TYPE_PING_REPLY = 0x02;
    
    // Entity packet types
    const unsigned char PACKET_TYPE_ENTITY_ADD = 0x10;
    const unsigned char PACKET_TYPE_ENTITY_EDIT = 0x11;
    const unsigned char PACKET_TYPE_ENTITY_ERASE = 0x12;
    const unsigned char PACKET_TYPE_ENTITY_QUERY = 0x15;
    const unsigned char PACKET_TYPE_OCTREE_STATS = 0x16;
    
    switch (packetType) {
        case PACKET_TYPE_DOMAIN_LIST:
            handleDomainListReply(data + 1, len - 1);
            break;
            
        case PACKET_TYPE_PING_REPLY:
            std::cout << "[OverteClient] Ping reply received from domain" << std::endl;
            break;
            
        case PACKET_TYPE_ENTITY_ADD: {
            // EntityAdd packet structure (simplified):
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
            
            // Create entity with a visible position spread out in front of user
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
        
        default:
            // Log unknown packet types for debugging
            if (packetType != PACKET_TYPE_PING && packetType != PACKET_TYPE_PING_REPLY) {
                std::cout << "[OverteClient] Unknown packet type: 0x" << std::hex << (int)packetType << std::dec << std::endl;
            }
            break;
    }
}

void OverteClient::handleDomainListReply(const char* data, size_t len) {
    // DomainList packet contains mixer endpoints
    // Format: sequence of [NodeType:u8][UUID:16bytes][PublicSocket:sockaddr][LocalSocket:sockaddr]
    std::cout << "[OverteClient] DomainList reply received (" << len << " bytes)" << std::endl;
    
    if (len < 1) return;
    
    // Parse number of nodes
    size_t offset = 0;
    
    while (offset + 1 < len) {
        unsigned char nodeType = data[offset++];
        
        // Skip UUID (16 bytes)
        if (offset + 16 > len) break;
        offset += 16;
        
        // Read public socket address (sockaddr_in or sockaddr_in6)
        if (offset + sizeof(sockaddr_in) > len) break;
        
        sockaddr_in publicAddr;
        std::memcpy(&publicAddr, data + offset, sizeof(sockaddr_in));
        offset += sizeof(sockaddr_in);
        
        // Skip local socket (same size)
        if (offset + sizeof(sockaddr_in) > len) break;
        offset += sizeof(sockaddr_in);
        
        // NodeType values from Overte:
        // 0 = DomainServer, 1 = EntityServer, 2 = Agent, 3 = AudioMixer, 4 = AvatarMixer, 5 = AssetServer, 6 = MessagesMixer, 7 = EntityScriptServer
        const unsigned char NODE_TYPE_ENTITY_SERVER = 1;
        const unsigned char NODE_TYPE_AVATAR_MIXER = 4;
        const unsigned char NODE_TYPE_AUDIO_MIXER = 3;
        
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &publicAddr.sin_addr, addrStr, sizeof(addrStr));
        int port = ntohs(publicAddr.sin_port);
        
        std::cout << "[OverteClient] Mixer discovered: type=" << (int)nodeType 
                  << " addr=" << addrStr << ":" << port << std::endl;
        
        if (nodeType == NODE_TYPE_ENTITY_SERVER && !m_entityServerReady) {
            // Update EntityServer connection to use discovered address
            std::cout << "[OverteClient] Using discovered EntityServer at " << addrStr << ":" << port << std::endl;
            
            // Update target address for EntityServer
            sockaddr_in* entityAddr = reinterpret_cast<sockaddr_in*>(&m_entityAddr);
            entityAddr->sin_family = AF_INET;
            entityAddr->sin_port = publicAddr.sin_port;
            entityAddr->sin_addr = publicAddr.sin_addr;
            m_entityAddrLen = sizeof(sockaddr_in);
            
            // Send EntityQuery to request all entities
            sendEntityQuery();
        }
    }
}

void OverteClient::sendDomainListRequest() {
    // Send DomainList request packet (PacketType 0x02 typically)
    if (!m_udpReady || m_udpFd == -1) return;
    
    const unsigned char PACKET_TYPE_DOMAIN_LIST_REQUEST = 0x02;
    char packet[1] = { static_cast<char>(PACKET_TYPE_DOMAIN_LIST_REQUEST) };
    
    ssize_t s = ::sendto(m_udpFd, packet, sizeof(packet), 0, 
                         reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
    if (s > 0) {
        std::cout << "[OverteClient] DomainList request sent" << std::endl;
    }
}

void OverteClient::sendEntityQuery() {
    if (m_entityFd < 0) return;
    
    const unsigned char PACKET_TYPE_ENTITY_QUERY = 0x15;
    
    // EntityQuery packet structure:
    // [PacketType:u8][ConicalViews:bool][CameraPosition:vec3][CameraOrientation:quat][CameraFov:float][CameraAspectRatio:float][CameraNearClip:float][CameraFarClip:float][CameraEyeOffsetPosition:vec3]
    // Simplified version: just send packet type + false for conical views (requests all entities)
    
    unsigned char packet[1 + 1]; // PacketType + ConicalViews flag
    packet[0] = PACKET_TYPE_ENTITY_QUERY;
    packet[1] = 0; // false - no conical frustum, send all entities
    
    ssize_t sent = sendto(m_entityFd, packet, sizeof(packet), 0, 
                         reinterpret_cast<const sockaddr*>(&m_entityAddr), m_entityAddrLen);
    
    if (sent > 0) {
        std::cout << "[OverteClient] Sent EntityQuery (all entities)" << std::endl;
        m_entityServerReady = true;
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
