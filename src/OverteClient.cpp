#include "OverteClient.hpp"
#include "NLPacketCodec.hpp"
#include "OverteAuth.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <zlib.h>
#include <endian.h>

using namespace std::chrono_literals;
using namespace Overte;

// Minimal QDataStream-like writer (Big Endian) for Qt wire format
namespace {
struct QtStream {
    std::vector<uint8_t> buf;
    void writeUInt8(uint8_t v) { buf.push_back(v); }
    void writeUInt16BE(uint16_t v) {
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    void writeUInt32BE(uint32_t v) {
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    void writeUInt64BE(uint64_t v) {
        for (int i = 7; i >= 0; --i) buf.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }
    void writeInt32BE(int32_t v) {
        writeUInt32BE(static_cast<uint32_t>(v));
    }
    void writeBytes(const uint8_t* d, size_t n) { buf.insert(buf.end(), d, d + n); }
    void writeQByteArray(const std::vector<uint8_t>& a) { writeUInt32BE(static_cast<uint32_t>(a.size())); writeBytes(a.data(), a.size()); }
    void writeQByteArrayFromString(const std::string& s) { std::vector<uint8_t> v(s.begin(), s.end()); writeQByteArray(v); }
    void writeQString(const std::string& s) {
        // QDataStream QString: quint32 length (chars), then UTF-16 BE code units
        writeUInt32BE(static_cast<uint32_t>(s.size()));
        for (unsigned char c : s) { writeUInt16BE(static_cast<uint16_t>(c)); }
    }
    static bool parseHex(const std::string& hex, uint64_t& out, size_t digits) {
        if (hex.size() < digits) return false; out = 0; for (size_t i = 0; i < digits; ++i) {
            char ch = hex[i]; uint8_t val;
            if (ch >= '0' && ch <= '9') val = ch - '0';
            else if (ch >= 'a' && ch <= 'f') val = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') val = ch - 'A' + 10;
            else return false; out = (out << 4) | val; }
        return true;
    }
    void writeQUuidFromString(const std::string& uuid) {
        // UUID string xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        std::string hex; hex.reserve(32);
        for (char c : uuid) if (c != '-') hex.push_back(c);
        if (hex.size() != 32) { // write zeros
            for (int i = 0; i < 16; ++i) buf.push_back(0); return;
        }
        uint64_t d1=0,d2=0,d3=0; // using 64 for parse then cast
        parseHex(hex.substr(0,8), d1, 8);
        parseHex(hex.substr(8,4), d2, 4);
        parseHex(hex.substr(12,4), d3, 4);
        writeUInt32BE(static_cast<uint32_t>(d1));
        writeUInt16BE(static_cast<uint16_t>(d2));
        writeUInt16BE(static_cast<uint16_t>(d3));
        // remaining 8 bytes
        for (int i = 0; i < 8; ++i) {
            uint64_t byteVal=0; parseHex(hex.substr(16 + i*2, 2), byteVal, 2);
            writeUInt8(static_cast<uint8_t>(byteVal & 0xFF));
        }
    }
};

static std::vector<uint8_t> qCompressLike(const std::vector<uint8_t>& input, int level = Z_BEST_SPEED) {
    // Produce Qt-like qCompress payload: 4-byte big-endian uncompressed size + zlib deflate stream
    uLongf destLen = compressBound(input.size());
    std::vector<uint8_t> comp(destLen);
    int rc = compress2(comp.data(), &destLen, input.data(), input.size(), level);
    if (rc != Z_OK) { destLen = 0; }
    comp.resize(destLen);
    std::vector<uint8_t> out;
    out.reserve(4 + comp.size());
    // 4-byte big-endian uncompressed size
    out.push_back(static_cast<uint8_t>((input.size() >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((input.size() >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((input.size() >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(input.size() & 0xFF));
    out.insert(out.end(), comp.begin(), comp.end());
    return out;
}
} // namespace

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

OverteClient::OverteClient(std::string domainUrl)
    : m_domainUrl(std::move(domainUrl)) {
}

OverteClient::~OverteClient() {
    // Destructor implementation (required for unique_ptr with forward-declared type)
}

bool OverteClient::login(const std::string& username, const std::string& password, const std::string& metaverseUrl) {
    if (!m_auth) {
        m_auth = std::make_unique<OverteAuth>();
    }
    
    bool success = m_auth->login(username, password, metaverseUrl);
    if (success) {
        m_username = username;
    }
    return success;
}

bool OverteClient::isAuthenticated() const {
    return m_auth && m_auth->isAuthenticated();
}

bool OverteClient::connect() {
    // Generate session UUID
    m_sessionUUID = generateUUID();
    std::cout << "[OverteClient] Session UUID: " << m_sessionUUID << std::endl;
    
    // Check for authentication credentials from environment
    const char* usernameEnv = std::getenv("OVERTE_USERNAME");
    const char* passwordEnv = std::getenv("OVERTE_PASSWORD");
    const char* metaverseEnv = std::getenv("OVERTE_METAVERSE");
    
    // TODO: OAuth authentication to metaverse server
    // Currently disabled because mv.overte.org doesn't expose /oauth/token endpoint
    // Overte uses web-based OAuth flow, not direct API authentication
    /*
    if (usernameEnv && passwordEnv) {
        std::string metaverseUrl = metaverseEnv ? metaverseEnv : "https://mv.overte.org";
        std::cout << "[OverteClient] Attempting login as " << usernameEnv << "..." << std::endl;
        if (login(usernameEnv, passwordEnv, metaverseUrl)) {
            std::cout << "[OverteClient] Successfully authenticated!" << std::endl;
        } else {
            std::cerr << "[OverteClient] Authentication failed, continuing as anonymous" << std::endl;
        }
    } else if (usernameEnv) {
        m_username = usernameEnv;
        std::cout << "[OverteClient] Username set (no password provided, signature auth not yet implemented)" << std::endl;
    }
    */
    
    if (usernameEnv) {
        std::cout << "[OverteClient] Note: Username '" << usernameEnv << "' provided but metaverse OAuth not yet implemented" << std::endl;
        std::cout << "[OverteClient] Continuing as anonymous user" << std::endl;
    }
    
    // Parse ws://host:port or host:port format
    std::string url = m_domainUrl;
    if (url.empty()) url = "ws://127.0.0.1:40102";
    if (url.rfind("ws://", 0) == 0) url = url.substr(5);
    
    // Parse host:port, potentially with path/coords (e.g., "host:40104/0,0,0/0,0,0,1")
    auto slashPos = url.find('/');
    if (slashPos != std::string::npos) {
        url = url.substr(0, slashPos); // Strip position/orientation coords
    }
    
    auto colon = url.find(':');
    m_host = colon == std::string::npos ? url : url.substr(0, colon);
    
    // If port is specified in URL, use it as UDP port (Overte domain format)
    // Otherwise default to 40102 for HTTP
    int urlPort = colon == std::string::npos ? 40102 : std::stoi(url.substr(colon + 1));
    
    // Check for environment override for UDP port (domain server UDP port)
    const char* portEnv = std::getenv("OVERTE_UDP_PORT");
    int udpPort = portEnv ? std::atoi(portEnv) : urlPort; // Use URL port as UDP if not overridden
    
    // HTTP port is typically UDP port - 2 (40102 for UDP 40104)
    m_port = udpPort - 2;
    
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
        // Seed a few demo entities with different types and properties
        OverteEntity cubeA;
        cubeA.id = m_nextEntityId++;
        cubeA.name = "CubeA";
        cubeA.type = EntityType::Box;
        cubeA.color = glm::vec3(1.0f, 0.3f, 0.3f); // Red cube
        cubeA.dimensions = glm::vec3(0.2f, 0.2f, 0.2f);
        cubeA.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 1.5f, -2.0f));
        
        OverteEntity sphereB;
        sphereB.id = m_nextEntityId++;
        sphereB.name = "SphereB";
        sphereB.type = EntityType::Sphere;
        sphereB.color = glm::vec3(0.3f, 1.0f, 0.3f); // Green sphere
        sphereB.dimensions = glm::vec3(0.15f, 0.15f, 0.15f);
        sphereB.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 1.5f, -2.0f));
        
        OverteEntity modelC;
        modelC.id = m_nextEntityId++;
        modelC.name = "ModelC";
        modelC.type = EntityType::Model;
        modelC.color = glm::vec3(0.3f, 0.3f, 1.0f); // Blue tint
        modelC.dimensions = glm::vec3(0.25f, 0.25f, 0.25f);
        // Leave modelUrl empty - primitive will be used based on type
        modelC.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.2f, -2.0f));
        
        m_entities.emplace(cubeA.id, cubeA);
        m_entities.emplace(sphereB.id, sphereB);
        m_entities.emplace(modelC.id, modelC);
        m_updateQueue.push_back(cubeA.id);
        m_updateQueue.push_back(sphereB.id);
        m_updateQueue.push_back(modelC.id);
        std::cout << "[OverteClient] Simulation mode enabled (STARWORLD_SIMULATE=1) with 3 demo entities" << std::endl;
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
    if (len < 6) return;  // NLPacket header is minimum 6 bytes
    
    // Parse NLPacket header
    NLPacket::Header header;
    const uint8_t* udata = reinterpret_cast<const uint8_t*>(data);
    if (!NLPacket::parseHeader(udata, len, header)) {
        std::cerr << "[OverteClient] Failed to parse NLPacket header" << std::endl;
        return;
    }
    
    PacketType packetType = NLPacket::getType(udata, len);
    std::cout << "[OverteClient] Domain packet type: " << static_cast<int>(packetType) 
              << " (0x" << std::hex << static_cast<int>(packetType) << std::dec << ")" 
              << " version: " << (int)header.version << std::endl;
    
    // Payload starts after header (6 bytes base, +2 if has source ID)
    const char* payload = data + 6;  // Assuming no source ID for now
    size_t payloadLen = len - 6;
    
    switch (packetType) {
        case PacketType::DomainList:
            handleDomainListReply(payload, payloadLen);
            break;
            
        case PacketType::DomainConnectionDenied:
            handleDomainConnectionDenied(payload, payloadLen);
            break;
            
        case PacketType::DomainServerRequireDTLS:
            std::cout << "[OverteClient] Domain server requires DTLS (not yet implemented)" << std::endl;
            break;
            
        case PacketType::PingReply:
            // Keep-alive ping reply
            std::cout << "[OverteClient] Ping reply received" << std::endl;
            break;
            
        case PacketType::ICEPing:
            // ICE ping for NAT traversal - reply immediately
            std::cout << "[OverteClient] ICE Ping received, sending reply" << std::endl;
            handleICEPing(payload, payloadLen);
            break;
            
        case PacketType::ICEPingReply:
            std::cout << "[OverteClient] ICE Ping Reply received" << std::endl;
            break;
            
        case PacketType::EntityData:
            std::cout << "[OverteClient] Received EntityData packet (" << payloadLen << " bytes)" << std::endl;
            parseEntityPacket(payload, payloadLen);
            break;
            
        case PacketType::EntityEditNack:
            std::cout << "[OverteClient] EntityEditNack received - entity creation/edit rejected" << std::endl;
            if (payloadLen > 0) {
                std::cout << "[OverteClient] Nack data (" << payloadLen << " bytes): ";
                for (size_t i = 0; i < std::min(payloadLen, size_t(32)); i++) {
                    printf("%02x ", (unsigned char)payload[i]);
                }
                std::cout << std::endl;
            }
            break;
            
        case PacketType::EntityQueryInitialResultsComplete:
            std::cout << "[OverteClient] Entity query initial results complete" << std::endl;
            break;
            
        default:
            // Log all unknown packet types to see what we're missing
            std::cout << "[OverteClient] Unknown/unhandled packet type: " << static_cast<int>(packetType) 
                      << " (0x" << std::hex << static_cast<int>(packetType) << std::dec << ")"
                      << " payload=" << payloadLen << " bytes" << std::endl;
            if (payloadLen > 0 && payloadLen <= 64) {
                std::cout << "[OverteClient] Payload hex: ";
                for (size_t i = 0; i < payloadLen; i++) {
                    printf("%02x ", (unsigned char)payload[i]);
                }
                std::cout << std::endl;
            }
            break;
    }
}

void OverteClient::parseEntityPacket(const char* data, size_t len) {
    // Overte packet structure (simplified):
    // - Byte 0: PacketType
    // - Following bytes: payload (varies by type)
    
    if (len < 1) return;
    
    // Debug: dump first bytes of packet
    std::cout << "[OverteClient] parseEntityPacket: " << len << " bytes, first 32: ";
    for (size_t i = 0; i < std::min(len, size_t(32)); i++) {
        printf("%02x ", (unsigned char)data[i]);
    }
    std::cout << std::endl;
    
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
            // EntityAdd packet structure (enhanced):
            // [type:u8][id:u64][name:null-terminated][position:3xf32][rotation:4xf32][dimensions:3xf32][model_url:null-terminated][texture_url:null-terminated][color:3xf32]
            if (len < 9) break; // need at least 1+8 bytes
            
            std::uint64_t entityId;
            std::memcpy(&entityId, data + 1, 8);
            
            // Parse name (null-terminated string after ID)
            size_t offset = 9;
            std::string name;
            while (offset < len && data[offset] != '\0') {
                name += data[offset++];
            }
            offset++; // skip null terminator
            if (name.empty()) name = "Entity_" + std::to_string(entityId);
            
            // Parse position (vec3 - 3 floats)
            glm::vec3 position(0.0f, 1.5f, -2.0f); // Default
            if (offset + 12 <= len) {
                std::memcpy(&position.x, data + offset, 4);
                std::memcpy(&position.y, data + offset + 4, 4);
                std::memcpy(&position.z, data + offset + 8, 4);
                offset += 12;
            }
            
            // Parse rotation (quaternion - 4 floats: x, y, z, w)
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f); // Identity (w, x, y, z in glm)
            if (offset + 16 <= len) {
                float qx, qy, qz, qw;
                std::memcpy(&qx, data + offset, 4);
                std::memcpy(&qy, data + offset + 4, 4);
                std::memcpy(&qz, data + offset + 8, 4);
                std::memcpy(&qw, data + offset + 12, 4);
                rotation = glm::quat(qw, qx, qy, qz); // glm uses w first
                offset += 16;
            }
            
            // Parse dimensions/scale (vec3 - 3 floats)
            glm::vec3 dimensions(0.1f, 0.1f, 0.1f); // Default
            if (offset + 12 <= len) {
                std::memcpy(&dimensions.x, data + offset, 4);
                std::memcpy(&dimensions.y, data + offset + 4, 4);
                std::memcpy(&dimensions.z, data + offset + 8, 4);
                offset += 12;
            }
            
            // Parse model URL (null-terminated string)
            std::string modelUrl;
            while (offset < len && data[offset] != '\0') {
                modelUrl += data[offset++];
            }
            offset++; // skip null terminator
            
            // Parse texture URL (null-terminated string)
            std::string textureUrl;
            while (offset < len && data[offset] != '\0') {
                textureUrl += data[offset++];
            }
            offset++; // skip null terminator
            
            // Parse color (vec3 RGB - 3 floats 0-1)
            glm::vec3 color(1.0f, 1.0f, 1.0f); // Default white
            if (offset + 12 <= len) {
                std::memcpy(&color.r, data + offset, 4);
                std::memcpy(&color.g, data + offset + 4, 4);
                std::memcpy(&color.b, data + offset + 8, 4);
                offset += 12;
            }
            
            // Parse entity type (optional, u8)
            EntityType entityType = EntityType::Box; // Default
            if (offset + 1 <= len) {
                uint8_t typeCode = static_cast<uint8_t>(data[offset++]);
                // Map Overte entity type codes to our enum
                // 0=Unknown, 1=Box, 2=Sphere, 3=Model, etc.
                if (typeCode <= static_cast<uint8_t>(EntityType::Material)) {
                    entityType = static_cast<EntityType>(typeCode);
                }
            }
            
            // Build transform matrix from position, rotation, scale
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, position);
            transform = transform * glm::mat4_cast(rotation);
            transform = glm::scale(transform, dimensions);
            
            // Create entity with all properties
            OverteEntity entity;
            entity.id = entityId;
            entity.name = name;
            entity.transform = transform;
            entity.type = entityType;
            entity.modelUrl = modelUrl;
            entity.textureUrl = textureUrl;
            entity.color = color;
            entity.dimensions = dimensions;
            entity.alpha = 1.0f; // Default fully opaque
            
            m_entities[entityId] = entity;
            m_updateQueue.push_back(entityId);
            
            std::cout << "[OverteClient] Entity added: " << name << " (id=" << entityId << ")" << std::endl;
            std::cout << "  Type: " << static_cast<int>(entityType) << std::endl;
            std::cout << "  Position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
            std::cout << "  Rotation: (" << rotation.x << ", " << rotation.y << ", " << rotation.z << ", " << rotation.w << ")" << std::endl;
            std::cout << "  Dimensions: (" << dimensions.x << ", " << dimensions.y << ", " << dimensions.z << ")" << std::endl;
            std::cout << "  Color: RGB(" << color.r << ", " << color.g << ", " << color.b << ")" << std::endl;
            if (!modelUrl.empty()) {
                std::cout << "  Model: " << modelUrl << std::endl;
            }
            if (!textureUrl.empty()) {
                std::cout << "  Texture: " << textureUrl << std::endl;
            }
            break;
        }
        
        case PACKET_TYPE_ENTITY_EDIT: {
            // EntityEdit packet: [type:u8][id:u64][flags:u8][property data...]
            if (len < 10) break; // Need type + id + flags
            
            std::uint64_t entityId;
            std::memcpy(&entityId, data + 1, 8);
            
            uint8_t flags = data[9];
            size_t offset = 10;
            
            const uint8_t HAS_POSITION = 0x01;
            const uint8_t HAS_ROTATION = 0x02;
            const uint8_t HAS_DIMENSIONS = 0x04;
            
            auto it = m_entities.find(entityId);
            if (it != m_entities.end()) {
                glm::vec3 position(0.0f);
                glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 dimensions(1.0f);
                
                // Extract current transform
                glm::vec3 scale;
                glm::quat currentRot;
                glm::vec3 currentPos;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(it->second.transform, scale, currentRot, currentPos, skew, perspective);
                
                position = currentPos;
                rotation = currentRot;
                dimensions = scale;
                
                // Update based on flags
                if (flags & HAS_POSITION) {
                    if (offset + 12 <= len) {
                        std::memcpy(&position.x, data + offset, 4);
                        std::memcpy(&position.y, data + offset + 4, 4);
                        std::memcpy(&position.z, data + offset + 8, 4);
                        offset += 12;
                    }
                }
                
                if (flags & HAS_ROTATION) {
                    if (offset + 16 <= len) {
                        float qx, qy, qz, qw;
                        std::memcpy(&qx, data + offset, 4);
                        std::memcpy(&qy, data + offset + 4, 4);
                        std::memcpy(&qz, data + offset + 8, 4);
                        std::memcpy(&qw, data + offset + 12, 4);
                        rotation = glm::quat(qw, qx, qy, qz);
                        offset += 16;
                    }
                }
                
                if (flags & HAS_DIMENSIONS) {
                    if (offset + 12 <= len) {
                        std::memcpy(&dimensions.x, data + offset, 4);
                        std::memcpy(&dimensions.y, data + offset + 4, 4);
                        std::memcpy(&dimensions.z, data + offset + 8, 4);
                        offset += 12;
                    }
                }
                
                // Rebuild transform
                glm::mat4 transform = glm::mat4(1.0f);
                transform = glm::translate(transform, position);
                transform = transform * glm::mat4_cast(rotation);
                transform = glm::scale(transform, dimensions);
                
                it->second.transform = transform;
                m_updateQueue.push_back(entityId);
                
                std::cout << "[OverteClient] Entity edited: id=" << entityId << " (flags=0x" << std::hex << (int)flags << std::dec << ")" << std::endl;
                if (flags & HAS_POSITION) {
                    std::cout << "  New position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
                }
                if (flags & HAS_ROTATION) {
                    std::cout << "  New rotation: (" << rotation.x << ", " << rotation.y << ", " << rotation.z << ", " << rotation.w << ")" << std::endl;
                }
                if (flags & HAS_DIMENSIONS) {
                    std::cout << "  New dimensions: (" << dimensions.x << ", " << dimensions.y << ", " << dimensions.z << ")" << std::endl;
                }
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

void OverteClient::handleICEPing(const char* data, size_t len) {
    // ICEPing packet format:
    // 1. ICE Client ID (16 bytes UUID)
    // 2. Ping type (uint8_t: 0=Local, 1=Public)
    
    if (len < 17) {
        std::cerr << "[OverteClient] ICEPing packet too short" << std::endl;
        return;
    }
    
    // Extract the ICE ID and ping type
    std::vector<uint8_t> iceID(data, data + 16);
    uint8_t pingType = static_cast<uint8_t>(data[16]);
    
    std::cout << "[OverteClient] ICEPing type=" << (int)pingType << std::endl;
    
    // Send ICEPingReply with the same ICE ID and ping type
    NLPacket reply(PacketType::ICEPingReply, 0, false);
    if (m_localID != 0) {
        reply.setSourceID(m_localID);
    }
    reply.setSequenceNumber(m_sequenceNumber++);
    
    // Write ICE ID and ping type
    reply.write(iceID.data(), iceID.size());
    reply.writeUInt8(pingType);
    
    const auto& replyData = reply.getData();
    ssize_t s = ::sendto(m_udpFd, replyData.data(), replyData.size(), 0,
                         reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
    
    if (s > 0) {
        std::cout << "[OverteClient] Sent ICEPingReply (" << s << " bytes)" << std::endl;
    } else {
        std::cerr << "[OverteClient] Failed to send ICEPingReply: " << strerror(errno) << std::endl;
    }
}

void OverteClient::handleDomainListReply(const char* data, size_t len) {
    // DomainList packet format (from Overte NodeList.cpp):
    // 1. Domain UUID (16 bytes)
    // 2. Session UUID (16 bytes) 
    // 3. Domain Local ID (16 bits)
    // 4. Permissions (32 bits)
    // 5. Authenticated (bool)
    // 6. Number of nodes (varies)
    // 7. Node data...
    
    std::cout << "[OverteClient] DomainList reply received (" << len << " bytes)" << std::endl;
    
    if (len < 37) { // Min: 16 (UUID) + 16 (session) + 2 (localID) + 4 (perms) + 1 (auth) = 39, but let's check for 37
        std::cout << "[OverteClient] DomainList packet too short" << std::endl;
        return;
    }
    
    size_t offset = 0;
    
    // Read domain UUID
    if (offset + 16 > len) return;
    char domainUUID[33];
    snprintf(domainUUID, sizeof(domainUUID),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             (unsigned char)data[offset], (unsigned char)data[offset+1],
             (unsigned char)data[offset+2], (unsigned char)data[offset+3],
             (unsigned char)data[offset+4], (unsigned char)data[offset+5],
             (unsigned char)data[offset+6], (unsigned char)data[offset+7],
             (unsigned char)data[offset+8], (unsigned char)data[offset+9],
             (unsigned char)data[offset+10], (unsigned char)data[offset+11],
             (unsigned char)data[offset+12], (unsigned char)data[offset+13],
             (unsigned char)data[offset+14], (unsigned char)data[offset+15]);
    offset += 16;
    
    std::cout << "[OverteClient] Domain UUID: " << domainUUID << std::endl;
    
    // Read session UUID
    if (offset + 16 > len) return;
    char sessionUUID[33];
    snprintf(sessionUUID, sizeof(sessionUUID),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             (unsigned char)data[offset], (unsigned char)data[offset+1],
             (unsigned char)data[offset+2], (unsigned char)data[offset+3],
             (unsigned char)data[offset+4], (unsigned char)data[offset+5],
             (unsigned char)data[offset+6], (unsigned char)data[offset+7],
             (unsigned char)data[offset+8], (unsigned char)data[offset+9],
             (unsigned char)data[offset+10], (unsigned char)data[offset+11],
             (unsigned char)data[offset+12], (unsigned char)data[offset+13],
             (unsigned char)data[offset+14], (unsigned char)data[offset+15]);
    offset += 16;
    
    std::cout << "[OverteClient] Session UUID: " << sessionUUID << std::endl;
    
    // Read domain local ID (16-bit)
    if (offset + 2 > len) return;
    uint16_t localID = ntohs(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    
    // Store our local ID for use in sourced packets
    m_localID = localID;
    
    std::cout << "[OverteClient] Local ID: " << localID << std::endl;
    
    // Read permissions (32-bit)
    if (offset + 4 > len) return;
    uint32_t permissions = ntohl(*reinterpret_cast<const uint32_t*>(data + offset));
    offset += 4;
    
    std::cout << "[OverteClient] Permissions: 0x" << std::hex << permissions << std::dec << std::endl;
    
    // Read authenticated flag
    if (offset + 1 > len) return;
    bool authenticated = data[offset++];
    
    std::cout << "[OverteClient] Authenticated: " << (authenticated ? "yes" : "no") << std::endl;
    
    // Read additional timing/metadata fields (from Overte's DomainServer::sendDomainListToNode)
    // These fields were added after the authenticated flag
    if (offset + 8 > len) {
        std::cout << "[OverteClient] Packet too short for timing fields" << std::endl;
        return;
    }
    
    // lastDomainCheckinTimestamp (uint64)
    uint64_t lastCheckinTimestamp;
    std::memcpy(&lastCheckinTimestamp, data + offset, 8);
    lastCheckinTimestamp = be64toh(lastCheckinTimestamp);
    offset += 8;
    
    if (offset + 8 > len) return;
    // currentTimestamp (uint64)
    uint64_t currentTimestamp;
    std::memcpy(&currentTimestamp, data + offset, 8);
    currentTimestamp = be64toh(currentTimestamp);
    offset += 8;
    
    if (offset + 8 > len) return;
    // processingTime (uint64)
    uint64_t processingTime;
    std::memcpy(&processingTime, data + offset, 8);
    processingTime = be64toh(processingTime);
    offset += 8;
    
    if (offset + 1 > len) return;
    // newConnection (bool)
    bool newConnection = data[offset++];
    
    std::cout << "[OverteClient] New connection: " << (newConnection ? "yes" : "no") << std::endl;
    
    // Now mark as connected since we got a valid DomainList
    m_domainConnected = true;
    
    // Clear previous assignment client list
    m_assignmentClients.clear();
    m_entityServerPort = 0;
    
    std::cout << "[OverteClient] Bytes remaining after header: " << (len - offset) << std::endl;
    std::cout << "[OverteClient] Remaining bytes (hex): ";
    for (size_t i = offset; i < std::min(offset + 40, len); i++) {
        printf("%02x ", (unsigned char)data[i]);
    }
    std::cout << std::endl;
    
    // Check if this might be a count field (QDataStream format often starts with a count)
    if (len - offset >= 4) {
        uint32_t possibleCount = ntohl(*reinterpret_cast<const uint32_t*>(data + offset));
        std::cout << "[OverteClient] First 4 bytes as uint32 (big-endian): " << possibleCount << std::endl;
    }
    if (len - offset >= 2) {
        uint16_t possibleCount16 = ntohs(*reinterpret_cast<const uint16_t*>(data + offset));
        std::cout << "[OverteClient] First 2 bytes as uint16 (big-endian): " << possibleCount16 << std::endl;
        
        // New observation: those 2 bytes might be flags or a node count
        // Let's interpret them as little-endian too
        uint16_t possibleCount16_le = *reinterpret_cast<const uint16_t*>(data + offset);
        std::cout << "[OverteClient] First 2 bytes as uint16 (little-endian): " << possibleCount16_le << std::endl;
        std::cout << "[OverteClient] As individual bytes: 0x" << std::hex << (int)(unsigned char)data[offset] 
                  << " 0x" << (int)(unsigned char)data[offset+1] << std::dec << std::endl;
    }
    
    // Parse assignment client nodes from the packet
    // Each node is serialized using QDataStream format (see Node.cpp operator<<)
    // Format per node:
    // - NodeType (qint8/char)
    // - UUID (16 bytes)
    // - PublicSocket.type (quint8)
    // - PublicSocket (QHostAddress [1 byte protocol + 4 bytes IPv4] + quint16 port)
    // - LocalSocket.type (quint8)
    // - LocalSocket (QHostAddress + quint16 port)
    // - Permissions (quint32)
    // - isReplicated (bool)
    // - localID (quint16)
    // - connectionSecretUUID (16 bytes) - added by DomainList packet
    
    std::cout << "[OverteClient] Parsing assignment clients..." << std::endl;
    
    while (offset < len) {
        AssignmentClient ac;
        
        // Read NodeType (qint8)
        if (offset + 1 > len) break;
        ac.type = static_cast<uint8_t>(data[offset++]);
        
        // Read UUID (16 bytes)
        if (offset + 16 > len) break;
        std::memcpy(ac.uuid.data(), data + offset, 16);
        offset += 16;
        
        // Read PublicSocket.type (quint8)
        if (offset + 1 > len) break;
        uint8_t publicSocketType = static_cast<uint8_t>(data[offset++]);
        
        // Read PublicSocket.address (QHostAddress)
        if (offset + 1 > len) break;
        uint8_t addressProtocol = static_cast<uint8_t>(data[offset++]);
        
        if (addressProtocol == 1) { // IPv4
            if (offset + 4 > len) break;
            uint32_t ipv4Addr;
            std::memcpy(&ipv4Addr, data + offset, 4);
            ipv4Addr = ntohl(ipv4Addr);
            offset += 4;
            
            // Read PublicSocket.port (quint16)
            if (offset + 2 > len) break;
            uint16_t publicPort = ntohs(*reinterpret_cast<const uint16_t*>(data + offset));
            offset += 2;
            
            // Store address
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&ac.address);
            addr->sin_family = AF_INET;
            addr->sin_addr.s_addr = htonl(ipv4Addr);
            addr->sin_port = htons(publicPort);
            ac.addressLen = sizeof(sockaddr_in);
            ac.port = publicPort;
            
        } else {
            std::cout << "[OverteClient] Unsupported address protocol: " << (int)addressProtocol << std::endl;
            break;
        }
        
        // Read LocalSocket.type (quint8)
        if (offset + 1 > len) break;
        uint8_t localSocketType = static_cast<uint8_t>(data[offset++]);
        (void)localSocketType; // unused for now
        
        // Read LocalSocket.address (QHostAddress)
        if (offset + 1 > len) break;
        uint8_t localAddressProtocol = static_cast<uint8_t>(data[offset++]);
        
        if (localAddressProtocol == 1) { // IPv4
            if (offset + 4 > len) break;
            offset += 4; // Skip local IP
            
            // Read LocalSocket.port (quint16)
            if (offset + 2 > len) break;
            offset += 2; // Skip local port
        } else {
            std::cout << "[OverteClient] Unsupported local address protocol: " << (int)localAddressProtocol << std::endl;
            break;
        }
        
        // Read Permissions (quint32)
        if (offset + 4 > len) break;
        offset += 4; // Skip permissions
        
        // Read isReplicated (bool)
        if (offset + 1 > len) break;
        offset++; // Skip isReplicated
        
        // Read localID (quint16)
        if (offset + 2 > len) break;
        offset += 2; // Skip localID
        
        // Read connectionSecretUUID (16 bytes) - this is added by DomainList packet
        if (offset + 16 > len) break;
        offset += 16; // Skip connectionSecretUUID
        
        // Store this assignment client
        m_assignmentClients.push_back(ac);
        
        // NodeType mapping (from Overte NodeType.h):
        // 'D' (0x44) = DomainServer
        // 'o' (0x6F) = EntityServer
        // 'I' (0x49) = Agent
        // 'M' (0x4D) = AudioMixer
        // 'W' (0x57) = AvatarMixer
        // 'A' (0x41) = AssetServer
        // 'm' (0x6D) = MessagesMixer
        // 'S' (0x53) = EntityScriptServer
        
        const char* nodeTypeName = "Unknown";
        switch (ac.type) {
            case 'D': nodeTypeName = "DomainServer"; break;
            case 'o': nodeTypeName = "EntityServer"; break;
            case 'I': nodeTypeName = "Agent"; break;
            case 'M': nodeTypeName = "AudioMixer"; break;
            case 'W': nodeTypeName = "AvatarMixer"; break;
            case 'A': nodeTypeName = "AssetServer"; break;
            case 'm': nodeTypeName = "MessagesMixer"; break;
            case 'S': nodeTypeName = "EntityScriptServer"; break;
        }
        
        char addrStr[INET_ADDRSTRLEN];
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(&ac.address);
        inet_ntop(AF_INET, &addr->sin_addr, addrStr, sizeof(addrStr));
        
        std::cout << "[OverteClient] Assignment client: " << nodeTypeName 
                  << " at " << addrStr << ":" << ac.port << std::endl;
        
        // If this is the EntityServer, store its address for EntityQuery
        if (ac.type == 'o') { // EntityServer
            m_entityServerAddr = ac.address;
            m_entityServerAddrLen = ac.addressLen;
            m_entityServerPort = ac.port;
            
            std::cout << "[OverteClient] Entity server found at " << addrStr << ":" << ac.port << std::endl;
        }
    }
    
    std::cout << "[OverteClient] Parsed " << m_assignmentClients.size() << " assignment clients" << std::endl;
    
    // Now send EntityQuery to the EntityServer (if we found one)
    if (m_entityServerPort != 0) {
        std::cout << "[OverteClient] Domain connected! Sending entity query to entity-server..." << std::endl;
        sendEntityQuery();
    } else {
        std::cout << "[OverteClient] Warning: No EntityServer found in assignment client list" << std::endl;
        std::cout << "[OverteClient] This might be expected for non-authenticated connections." << std::endl;
        
        // The first DomainList reply might not include assignment clients
        // Request an updated DomainList now that the server knows our interests
        std::cout << "[OverteClient] Requesting updated DomainList to get assignment clients..." << std::endl;
        sendDomainListRequest();
        
        // Modern Overte: also try sending EntityQuery directly to domain server
        // The domain server may forward it to the EntityServer or respond directly
        std::cout << "[OverteClient] Sending EntityQuery to domain server as fallback..." << std::endl;
        m_entityServerPort = 0; // Will use domain server address
        sendEntityQuery();
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
    
    // Create NLPacket with DomainConnectRequest type and correct version
    NLPacket packet(PacketType::DomainConnectRequest, PacketVersions::DomainConnectRequest_SocketTypes, true);
    packet.setSequenceNumber(m_sequenceNumber++);
    
    // Build payload using Qt wire format (match Overte's NodeList.cpp structure exactly)
    QtStream qs;
    
    // 1. UUID
    qs.writeQUuidFromString(m_sessionUUID);
    
    // 2. Protocol signature (QByteArray)
    auto protocolSig = NLPacket::computeProtocolVersionSignature();
    qs.writeQByteArray(protocolSig);
    
    // 3. Hardware/MAC address (QString) - empty if unknown
    std::string macAddr = "";
    qs.writeQString(macAddr);
    
    // 4. Machine fingerprint (QUuid)
    qs.writeQUuidFromString(m_sessionUUID);
    
    // 5. Compressed system info (QByteArray)
    std::string sysJson = "{\"computer\":{\"OS\":\"Linux\"},\"cpus\":[{\"model\":\"Stardust\"}],\"memory\":4096,\"nics\":[],\"gpus\":[],\"displays\":[]}";
    std::vector<uint8_t> sysBytes(sysJson.begin(), sysJson.end());
    auto sysCompressed = qCompressLike(sysBytes, Z_BEST_SPEED);
    qs.writeQByteArray(sysCompressed);
    
    // 6. Connect reason (quint32) - 0 = Unknown
    qs.writeUInt32BE(0);
    
    // 7. Previous connection uptime (quint64) - 0 for first connection
    qs.writeUInt64BE(0);
    
    // 8. Current timestamp in microseconds (quint64) as lastPingTimestamp
    auto nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    qs.writeUInt64BE(static_cast<uint64_t>(nowUs));
    
    // 9. Node type / owner type (NodeType_t)
    qs.writeUInt8(static_cast<uint8_t>('I')); // Agent
    
    // Determine local UDP socket address/port (bind address if needed)
    uint32_t localIPv4 = 0x7F000001; // 127.0.0.1 fallback
    uint16_t localPort = 0;
    sockaddr_storage localSs{}; socklen_t localLen = sizeof(localSs);
    if (::getsockname(m_udpFd, reinterpret_cast<sockaddr*>(&localSs), &localLen) == 0) {
        if (localSs.ss_family == AF_INET) {
            auto* sin = reinterpret_cast<sockaddr_in*>(&localSs);
            localIPv4 = ntohl(sin->sin_addr.s_addr);
            localPort = ntohs(sin->sin_port);
        }
    }
    // Helper lambda to write QHostAddress (IPv4) in QDataStream format: [protocol:quint8=1][IPv4:quint32]
    auto writeQHostAddressIPv4 = [&qs](uint32_t hostOrderIPv4){
        // QDataStream for QHostAddress writes a protocol tag (quint8).
        // QAbstractSocket::NetworkLayerProtocol: AnyIPProtocol=0, IPv4Protocol=1, IPv6Protocol=2.
        // We want IPv4Protocol = 1.
        qs.writeUInt8(1);
        qs.writeUInt32BE(hostOrderIPv4);
    };

    // 10. Public socket: type (quint8) + SockAddr (QHostAddress + quint16 port, WITHOUT socket type per SockAddr QDataStream operator)
    qs.writeUInt8(1); // SocketType::UDP
    writeQHostAddressIPv4(localIPv4); // using local as placeholder for public
    qs.writeUInt16BE(localPort); // actual local port (might be 0 if not yet bound)

    // 11. Local socket: type (quint8) + SockAddr
    qs.writeUInt8(1); // SocketType::UDP
    writeQHostAddressIPv4(localIPv4);
    qs.writeUInt16BE(localPort);
    
    // 12. Node types of interest (QList<NodeType_t>)
    // Write as Qt container: size (qint32) + elements (quint8) -- include a few mixers we want
    // Typical Interface requests at least AvatarMixer, AudioMixer, EntityServer
    const uint8_t interestList[] = { static_cast<uint8_t>('W'), /* AvatarMixer */ static_cast<uint8_t>('M'), /* AudioMixer */ static_cast<uint8_t>('o') /* EntityServer */ };
    qs.writeInt32BE(static_cast<int32_t>(sizeof(interestList)));
    for (auto b : interestList) qs.writeUInt8(b);
    
    // 13. Place name (QString) - empty
    qs.writeQString("");
    
    // 14. Directory services username (QString) - empty for now
    // TODO: Username sending causes domain server to not respond
    // const char* usernameEnv = std::getenv("OVERTE_USERNAME");
    // std::string dsUsername = usernameEnv ? usernameEnv : "";
    qs.writeQString("");  // Always send empty for now
    
    // 15. Username signature (QString) - empty (no keypair authentication)
    qs.writeQString("");
    
    // 16. Domain username (QString) - send empty for compatibility
    qs.writeQString("");
    
    // 17. Domain access token:refreshToken (QString) - send empty for compatibility  
    qs.writeQString("");

    // Append payload to packet
    if (!qs.buf.empty()) packet.write(qs.buf.data(), qs.buf.size());
    
    const auto& data = packet.getData();
    ssize_t s = ::sendto(m_udpFd, data.data(), data.size(), 0, 
                         reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
    if (s > 0) {
        std::cout << "[OverteClient] DomainConnectRequest sent (" << s << " bytes, seq=" << (m_sequenceNumber-1) << ")" << std::endl;
        std::cout << "[OverteClient]   Session UUID: " << m_sessionUUID << std::endl;
    // Print MD5 signature in hex for diff against reference Overte client
    std::ostringstream md5hex; md5hex << std::hex << std::setfill('0');
    for (uint8_t byte : protocolSig) md5hex << std::setw(2) << (int)byte;
        // Base64 encode MD5 for comparison with Overte's protocolVersionsSignatureBase64()
        auto base64Encode = [](const std::vector<uint8_t>& in){
            static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out; out.reserve(((in.size()+2)/3)*4);
            size_t i=0; while(i<in.size()){ uint32_t val=0; int bytes=0; for(int j=0;j<3;++j){ val <<=8; if(i<in.size()){ val|=in[i++]; ++bytes; } }
                int pad = 3 - bytes; for(int k=0;k<4-pad;++k){ int idx = (val >> (18 - k*6)) & 0x3F; out.push_back(tbl[idx]); }
                for(int k=0;k<pad;++k) out.push_back('='); }
            return out; };
        std::string md5Base64 = base64Encode(protocolSig);
        std::cout << "[OverteClient]   Protocol signature: " << protocolSig.size() << " bytes (MD5)" << std::endl;
        std::cout << "[OverteClient]   Protocol signature (hex): " << md5hex.str() << std::endl;
        std::cout << "[OverteClient]   Protocol signature (base64): " << md5Base64 << std::endl;
        
        // Detailed payload breakdown
        std::cout << "[OverteClient]   Payload size: " << qs.buf.size() << " bytes" << std::endl;
        std::cout << "[OverteClient]   >>> Payload (QDataStream format):" << std::endl;
        std::cout << "[OverteClient]       UUID (16 bytes)" << std::endl;
        std::cout << "[OverteClient]       Protocol sig length (4 bytes): ";
        if (qs.buf.size() >= 20) {
            uint32_t sigLen = (qs.buf[16] << 24) | (qs.buf[17] << 16) | (qs.buf[18] << 8) | qs.buf[19];
            std::cout << sigLen << std::endl;
            std::cout << "[OverteClient]       Protocol sig data (" << sigLen << " bytes at offset 20): ";
            for (size_t i = 20; i < 20 + sigLen && i < qs.buf.size(); ++i) {
                printf("%02x", qs.buf[i]);
            }
            std::cout << std::endl;
            
            // Compare with what we computed
            std::cout << "[OverteClient]       Expected signature:                                   ";
            for (uint8_t byte : protocolSig) printf("%02x", byte);
            std::cout << std::endl;
            
            bool match = true;
            for (size_t i = 0; i < protocolSig.size() && i < sigLen; ++i) {
                if (qs.buf[20 + i] != protocolSig[i]) {
                    match = false;
                    break;
                }
            }
            std::cout << "[OverteClient]       Signatures " << (match ? "MATCH ✓" : "MISMATCH ✗") << std::endl;
        }
        
        // Hex dump first 128 bytes
        std::cout << "[OverteClient] >>> Full packet hex (first 128 bytes):" << std::endl;
        for (size_t i = 0; i < std::min(size_t(128), data.size()); ++i) {
            if (i > 0 && i % 16 == 0) std::cout << std::endl << "                   ";
            printf("%02x ", data[i]);
        }
        std::cout << std::endl;
    } else {
        std::cerr << "[OverteClient] Failed to send domain connect request: " << strerror(errno) << std::endl;
    }
}

void OverteClient::sendDomainListRequest() {
    // Send DomainList request packet using NLPacket format
    if (!m_udpReady || m_udpFd == -1) return;
    
    // Create NLPacket with DomainListRequest type and correct version
    NLPacket packet(PacketType::DomainListRequest, PacketVersions::DomainListRequest_SocketTypes, true);
    packet.setSequenceNumber(m_sequenceNumber++);
    
    // DomainListRequest has no payload, just the header
    
    const auto& data = packet.getData();
    ssize_t s = ::sendto(m_udpFd, data.data(), data.size(), 0, 
                         reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
    if (s > 0) {
        std::cout << "[OverteClient] DomainListRequest sent (seq=" << (m_sequenceNumber-1) << ")" << std::endl;
    } else {
        std::cerr << "[OverteClient] Failed to send domain list request: " << strerror(errno) << std::endl;
    }
}

void OverteClient::sendPing(int fd, const sockaddr_storage& addr, socklen_t addrLen) {
    // Create NLPacket for Ping with correct version
    NLPacket packet(PacketType::Ping, PacketVersions::Ping_IncludeConnectionID, false);
    // Include our local ID if we have one (sourced packet)
    if (m_localID != 0) {
        packet.setSourceID(m_localID);
    }
    packet.setSequenceNumber(m_sequenceNumber++);
    
    // Add timestamp (microseconds since epoch)
    auto now = std::chrono::system_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    packet.writeUInt64(micros);
    
    // Ping type (0 = local, 1 = public)
    packet.writeUInt8(0);
    
    const auto& data = packet.getData();
    ssize_t s = ::sendto(fd, data.data(), data.size(), 0, 
                         reinterpret_cast<const sockaddr*>(&addr), addrLen);
    if (s < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
        std::cerr << "[OverteClient] Ping send failed: " << strerror(errno) << std::endl;
    }
}

void OverteClient::sendEntityQuery() {
    if (!m_udpReady || m_udpFd == -1) return;
    
    // Use entity server address if available, otherwise fall back to domain server
    const sockaddr_storage* targetAddr = m_entityServerPort != 0 ? 
        &m_entityServerAddr : &m_udpAddr;
    socklen_t targetAddrLen = m_entityServerPort != 0 ?
        m_entityServerAddrLen : m_udpAddrLen;
    
    // Create EntityQuery packet (PacketType::EntityQuery = 0x29)
    NLPacket packet(PacketType::EntityQuery, 0, true);
    // Include our local ID (sourced packet)
    if (m_localID != 0) {
        packet.setSourceID(m_localID);
    }
    packet.setSequenceNumber(m_sequenceNumber++);
    
    // OctreeQuery payload format (from OctreeQuery::getBroadcastData):
    // 1. Connection ID (uint16)
    // 2. Number of frustums (uint8) - 0 for requesting all entities
    // 3. Frustum data (if numFrustums > 0) - we skip this
    // 4. Max octree packets per second (int32)
    // 5. Octree size scale (float32)
    // 6. Boundary level adjust (int32)
    // 7. JSON parameters size (uint16)
    // 8. JSON parameters (if size > 0)
    // 9. Query flags (uint16)
    
    std::vector<uint8_t> payload;
    auto writeU16 = [&](uint16_t v) {
        payload.push_back((v >> 8) & 0xFF);
        payload.push_back(v & 0xFF);
    };
    auto writeU8 = [&](uint8_t v) { payload.push_back(v); };
    auto writeI32 = [&](int32_t v) {
        payload.push_back((v >> 24) & 0xFF);
        payload.push_back((v >> 16) & 0xFF);
        payload.push_back((v >> 8) & 0xFF);
        payload.push_back(v & 0xFF);
    };
    auto writeF32 = [&](float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(float));
        writeI32(static_cast<int32_t>(bits));
    };
    
    // 1. Connection ID - use 0 for initial query
    static uint16_t connectionID = 0;
    writeU16(connectionID);
    
    // 2. Number of frustums - 0 to request all entities
    writeU8(0);
    
    // 3. No frustum data since numFrustums = 0
    
    // 4. Max octree PPS - 3000 is typical
    writeI32(3000);
    
    // 5. Octree size scale - 1.0 is default
    writeF32(1.0f);
    
    // 6. Boundary level adjust - 0 is default
    writeI32(0);
    
    // 7. JSON parameters size - 0 (no filters)
    writeU16(0);
    
    // 8. No JSON parameters
    
    // 9. Query flags - 0x1 = WantInitialCompletion
    writeU16(0x1);
    
    // Write payload to packet
    if (!payload.empty()) {
        packet.write(payload.data(), payload.size());
    }
    
    const auto& data = packet.getData();
    ssize_t s = ::sendto(m_udpFd, data.data(), data.size(), 0, 
                         reinterpret_cast<const sockaddr*>(targetAddr), targetAddrLen);
    
    if (s > 0) {
        char addrStr[INET_ADDRSTRLEN] = "unknown";
        if (targetAddr->ss_family == AF_INET) {
            const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(targetAddr);
            inet_ntop(AF_INET, &sin->sin_addr, addrStr, sizeof(addrStr));
        }
        
        const char* targetName = (m_entityServerPort != 0) ? "entity-server" : "domain-server";
        std::cout << "[OverteClient] Sent EntityQuery to " << targetName 
                  << " (" << addrStr << ":" << ntohs(reinterpret_cast<const sockaddr_in*>(targetAddr)->sin_port)
                  << ", " << s << " bytes, seq=" << (m_sequenceNumber-1) << ")" << std::endl;
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

void OverteClient::createEntity(const std::string& name, EntityType type, const glm::vec3& position,
                                const glm::vec3& dimensions, const glm::vec3& color) {
    if (!m_udpReady || m_udpFd == -1) {
        std::cerr << "[OverteClient] Cannot create entity: not connected" << std::endl;
        return;
    }
    
    if (m_localID == 0) {
        std::cerr << "[OverteClient] Cannot create entity: no local ID assigned yet" << std::endl;
        return;
    }
    
    std::cout << "[OverteClient] Creating entity: " << name << " at (" 
              << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    
    // Create EntityAdd packet (PacketType::EntityAdd = 0x3A)
    NLPacket packet(PacketType::EntityAdd, 0, true);
    packet.setSourceID(m_localID);
    packet.setSequenceNumber(m_sequenceNumber++);
    
    // EntityAdd packet format (simplified - basic properties only):
    // 1. Entity type (uint8)
    // 2. Creation time (uint64 microseconds since epoch)
    // 3. Last edited time (uint64)
    // 4. Entity ID flags (uint8) - 0x00 for server-generated ID
    // 5. Entity properties encoded as key-value pairs
    
    std::vector<uint8_t> payload;
    
    // Helper lambdas for writing data in network byte order
    auto writeU8 = [&](uint8_t v) { payload.push_back(v); };
    auto writeU16 = [&](uint16_t v) {
        payload.push_back((v >> 8) & 0xFF);
        payload.push_back(v & 0xFF);
    };
    auto writeU32 = [&](uint32_t v) {
        payload.push_back((v >> 24) & 0xFF);
        payload.push_back((v >> 16) & 0xFF);
        payload.push_back((v >> 8) & 0xFF);
        payload.push_back(v & 0xFF);
    };
    auto writeU64 = [&](uint64_t v) {
        for (int i = 7; i >= 0; --i) {
            payload.push_back((v >> (i * 8)) & 0xFF);
        }
    };
    auto writeF32 = [&](float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(float));
        writeU32(bits);
    };
    auto writeVec3 = [&](const glm::vec3& v) {
        writeF32(v.x);
        writeF32(v.y);
        writeF32(v.z);
    };
    auto writeString = [&](const std::string& s) {
        writeU16(static_cast<uint16_t>(s.length()));
        for (char c : s) {
            payload.push_back(static_cast<uint8_t>(c));
        }
    };
    
    // 1. Entity type - convert our EntityType to Overte's entity type codes
    uint8_t overtypeType = 0;
    switch (type) {
        case EntityType::Box: overtypeType = 1; break;
        case EntityType::Sphere: overtypeType = 2; break;
        case EntityType::Model: overtypeType = 3; break;
        case EntityType::Shape: overtypeType = 4; break;
        default: overtypeType = 1; break; // Default to Box
    }
    writeU8(overtypeType);
    
    // 2. Creation time (current time in microseconds)
    auto now = std::chrono::system_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    writeU64(static_cast<uint64_t>(micros));
    
    // 3. Last edited time (same as creation time)
    writeU64(static_cast<uint64_t>(micros));
    
    // 4. Entity ID flags - 0x00 means let server assign ID
    writeU8(0x00);
    
    // 5. Entity properties (encoded as a property list)
    // Property encoding format: property ID (uint16) + property data
    // Common property IDs (from EntityItemProperties.h):
    // - PROP_POSITION = 0x01
    // - PROP_DIMENSIONS = 0x02
    // - PROP_ROTATION = 0x03
    // - PROP_COLOR = 0x0C
    // - PROP_NAME = 0x1F
    
    // For simplicity, we'll encode a minimal set of properties
    // Overte uses a compact property encoding with flags, but we'll use a simpler approach
    
    // Name property (PROP_NAME = 0x1F = 31)
    writeU16(0x1F);
    writeString(name);
    
    // Position property (PROP_POSITION = 0x01 = 1)
    writeU16(0x01);
    writeVec3(position);
    
    // Dimensions property (PROP_DIMENSIONS = 0x02 = 2)
    writeU16(0x02);
    writeVec3(dimensions);
    
    // Color property (PROP_COLOR = 0x0C = 12)
    // Overte uses RGB values 0-255
    writeU16(0x0C);
    writeU8(static_cast<uint8_t>(color.r * 255.0f));
    writeU8(static_cast<uint8_t>(color.g * 255.0f));
    writeU8(static_cast<uint8_t>(color.b * 255.0f));
    
    // End of properties marker (property ID = 0xFFFF)
    writeU16(0xFFFF);
    
    // Write payload to packet
    if (!payload.empty()) {
        packet.write(payload.data(), payload.size());
    }
    
    const auto& data = packet.getData();
    ssize_t s = ::sendto(m_udpFd, data.data(), data.size(), 0,
                         reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
    
    if (s > 0) {
        std::cout << "[OverteClient] Sent EntityAdd (" << s << " bytes, seq=" << (m_sequenceNumber-1) << ")" << std::endl;
    } else {
        std::cerr << "[OverteClient] Failed to send EntityAdd: " << strerror(errno) << std::endl;
    }
}
