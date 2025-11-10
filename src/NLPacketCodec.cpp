// NLPacketCodec.cpp
#include "NLPacketCodec.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <openssl/md5.h>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <iostream>

namespace Overte {

namespace {

// Control bit masks for sequence number field
constexpr uint32_t CONTROL_BIT_MASK = 0x80000000;      // Bit 31
constexpr uint32_t RELIABLE_BIT_MASK = 0x40000000;     // Bit 30  
constexpr uint32_t MESSAGE_BIT_MASK = 0x20000000;      // Bit 29
constexpr uint32_t OBFUSCATION_MASK = 0x18000000;      // Bits 27-28
constexpr uint32_t SEQUENCE_NUMBER_MASK = 0x07FFFFFF;  // Bits 0-26

} // anonymous namespace

NLPacket::NLPacket(PacketType type, PacketVersion version, bool isReliable)
    : m_type(type)
    , m_version(version)
    , m_isReliable(isReliable)
{
    // Determine header size (sourced packets have LocalID)
    m_isSourced = false;  // Most client packets aren't sourced
    m_headerSize = m_isSourced ? SOURCED_HEADER_SIZE : BASE_HEADER_SIZE;
    
    // Reserve space for header
    m_data.resize(m_headerSize);
    writeHeader();
}

void NLPacket::writeHeader() {
    size_t offset = 0;
    
    // Write sequence number and flags
    uint32_t seqAndFlags = m_sequenceNumber & SEQUENCE_NUMBER_MASK;
    if (m_isReliable) {
        seqAndFlags |= RELIABLE_BIT_MASK;
    }
    // Convert to network byte order
    uint32_t netSeqAndFlags = htonl(seqAndFlags);
    std::memcpy(m_data.data() + offset, &netSeqAndFlags, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    // Write packet type
    m_data[offset++] = static_cast<uint8_t>(m_type);
    
    // Write version
    m_data[offset++] = m_version;
    
    // Write source ID if sourced
    if (m_isSourced) {
        uint16_t netSourceID = htons(m_sourceID);
        std::memcpy(m_data.data() + offset, &netSourceID, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }
}

void NLPacket::write(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_data.insert(m_data.end(), bytes, bytes + size);
}

void NLPacket::writeUInt8(uint8_t value) {
    m_data.push_back(value);
}

void NLPacket::writeUInt16(uint16_t value) {
    uint16_t netValue = htons(value);
    write(&netValue, sizeof(netValue));
}

void NLPacket::writeUInt32(uint32_t value) {
    uint32_t netValue = htonl(value);
    write(&netValue, sizeof(netValue));
}

void NLPacket::writeUInt64(uint64_t value) {
    // Network byte order for 64-bit (big-endian)
    uint64_t netValue = 
        ((value & 0xFF00000000000000ULL) >> 56) |
        ((value & 0x00FF000000000000ULL) >> 40) |
        ((value & 0x0000FF0000000000ULL) >> 24) |
        ((value & 0x000000FF00000000ULL) >> 8)  |
        ((value & 0x00000000FF000000ULL) << 8)  |
        ((value & 0x0000000000FF0000ULL) << 24) |
        ((value & 0x000000000000FF00ULL) << 40) |
        ((value & 0x00000000000000FFULL) << 56);
    write(&netValue, sizeof(netValue));
}

void NLPacket::writeString(const std::string& str, bool nullTerminated) {
    write(str.data(), str.size());
    if (nullTerminated) {
        writeUInt8(0);
    }
}

void NLPacket::setSequenceNumber(SequenceNumber seq) {
    m_sequenceNumber = seq & SEQUENCE_NUMBER_MASK;
    writeHeader();
}

void NLPacket::setSourceID(LocalID id) {
    m_sourceID = id;
    m_isSourced = true;
    // Resize if needed
    if (m_headerSize != SOURCED_HEADER_SIZE) {
        m_headerSize = SOURCED_HEADER_SIZE;
        m_data.resize(m_headerSize);
    }
    writeHeader();
}

bool NLPacket::parseHeader(const uint8_t* data, size_t size, Header& header) {
    if (size < BASE_HEADER_SIZE) {
        return false;
    }
    
    size_t offset = 0;
    
    // Read sequence and flags
    uint32_t netSeqAndFlags;
    std::memcpy(&netSeqAndFlags, data + offset, sizeof(uint32_t));
    header.sequenceAndFlags = ntohl(netSeqAndFlags);
    offset += sizeof(uint32_t);
    
    // Read packet type
    header.type = static_cast<PacketType>(data[offset++]);
    
    // Read version
    header.version = data[offset++];
    
    // Read source ID if present (check if packet is sourced)
    if (size >= SOURCED_HEADER_SIZE) {
        uint16_t netSourceID;
        std::memcpy(&netSourceID, data + offset, sizeof(uint16_t));
        header.sourceID = ntohs(netSourceID);
    } else {
        header.sourceID = NULL_LOCAL_ID;
    }
    
    return true;
}

PacketType NLPacket::getType(const uint8_t* data, size_t size) {
    if (size < sizeof(uint32_t) + 1) {
        return PacketType::Unknown;
    }
    return static_cast<PacketType>(data[sizeof(uint32_t)]);
}

namespace {

// --- Helpers to parse Overte header enums to ensure exact version numbers ---
std::string readFileToString(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return {};
    std::ostringstream ss; ss << in.rdbuf();
    return ss.str();
}

std::unordered_map<std::string, int> parseEnumValues(const std::string& content, const std::string& enumName) {
    std::unordered_map<std::string, int> values;
    std::string startToken = "enum class " + enumName;
    auto startPos = content.find(startToken);
    if (startPos == std::string::npos) return values;
    auto bracePos = content.find('{', startPos);
    if (bracePos == std::string::npos) return values;
    auto endPos = content.find("};", bracePos);
    if (endPos == std::string::npos) return values;
    std::string body = content.substr(bracePos + 1, endPos - bracePos - 1);

    int current = -1;
    std::istringstream lines(body);
    std::string line;
    while (std::getline(lines, line)) {
        // strip comments
        auto cpos = line.find("//");
        if (cpos != std::string::npos) line = line.substr(0, cpos);
        // trim
        auto notspace = [](int ch){ return !std::isspace(ch); };
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), notspace));
        line.erase(std::find_if(line.rbegin(), line.rend(), notspace).base(), line.end());
        if (line.empty()) continue;
        // split by comma; a line may have trailing comma
        auto comma = line.find(',');
        std::string token = (comma == std::string::npos) ? line : line.substr(0, comma);
        // handle assignments
        auto eq = token.find('=');
        std::string name = token;
        if (eq != std::string::npos) {
            name = token.substr(0, eq);
            std::string val = token.substr(eq + 1);
            // trim name
            name.erase(name.begin(), std::find_if(name.begin(), name.end(), notspace));
            name.erase(std::find_if(name.rbegin(), name.rend(), notspace).base(), name.end());
            // trim val
            val.erase(val.begin(), std::find_if(val.begin(), val.end(), notspace));
            val.erase(std::find_if(val.rbegin(), val.rend(), notspace).base(), val.end());
            // numeric (no hex used in these enums)
            try { current = std::stoi(val); } catch (...) { continue; }
        } else {
            // trim name
            name.erase(name.begin(), std::find_if(name.begin(), name.end(), notspace));
            name.erase(std::find_if(name.rbegin(), name.rend(), notspace).base(), name.end());
            current = current + 1;
        }
        if (!name.empty()) values[name] = current;
    }
    return values;
}

int parsePacketTypeCount(const std::string& content) {
    // Count identifiers in PacketTypeEnum::Value until NUM_PACKET_TYPE
    auto pos = content.find("enum class Value : uint8_t");
    if (pos == std::string::npos) return 106; // fallback
    auto brace = content.find('{', pos);
    if (brace == std::string::npos) return 106;
    auto end = content.find("NUM_PACKET_TYPE", brace);
    if (end == std::string::npos) return 106;
    std::string body = content.substr(brace + 1, end - brace - 1);
    int count = 0;
    std::istringstream lines(body);
    std::string line;
    while (std::getline(lines, line)) {
        auto cpos = line.find("//"); if (cpos != std::string::npos) line = line.substr(0, cpos);
        auto notspace = [](int ch){ return !std::isspace(ch); };
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), notspace));
        line.erase(std::find_if(line.rbegin(), line.rend(), notspace).base(), line.end());
        if (line.empty()) continue;
        if (line.find('=') != std::string::npos) {
            // handle explicit value lines (rare in this enum)
            count++;
        } else if (line.find(',') != std::string::npos) {
            count++;
        }
    }
    return count; // this should equal NUM_PACKET_TYPE
}

void ensureVersionTable(uint8_t& vAvatarRemoveAttachments,
                        uint8_t& vAvatarTraitsAck,
                        uint8_t& vEntityLastPacket,
                        uint8_t& vEntityParticleSpin,
                        uint8_t& vAssetBakingTextureMeta,
                        uint8_t& vEntityScriptClientCallable,
                        uint8_t& vEntityQueryCbor,
                        uint8_t& vAvatarQueryConical,
                        uint8_t& vDomainServerAddedNodeSocketTypes,
                        uint8_t& vDomainListSocketTypes,
                        uint8_t& vDomainListRequestSocketTypes,
                               uint8_t& vDomainConnectionDeniedExtraInfo,
                               uint8_t& vPingIncludeConnID,
                               uint8_t& vIcePingSendPeerID,
                               uint8_t& vAudioStopInjectors,
                               int& numPacketTypes)
{
    static bool inited = false;
    static uint8_t s_vAvatarRemoveAttachments, s_vAvatarTraitsAck, s_vEntityLastPacket,
                   s_vEntityParticleSpin, s_vAssetBakingTextureMeta, s_vEntityScriptClientCallable, s_vEntityQueryCbor,
                   s_vAvatarQueryConical, s_vDomainServerAddedNodeSocketTypes, s_vDomainListSocketTypes,
                   s_vDomainListRequestSocketTypes, s_vDomainConnectionDeniedExtraInfo,
                   s_vPingIncludeConnID, s_vIcePingSendPeerID, s_vAudioStopInjectors;
    static int s_numPacketTypes;
    if (!inited) {
        std::string path = "third_party/overte-src/libraries/networking/src/udt/PacketHeaders.h";
        auto content = readFileToString(path);
        if (!content.empty()) {
            auto avatar = parseEnumValues(content, "AvatarMixerPacketVersion");
            auto entity = parseEnumValues(content, "EntityVersion");
            auto asset = parseEnumValues(content, "AssetServerPacketVersion");
            auto entScript = parseEnumValues(content, "EntityScriptCallMethodVersion");
            auto entQuery = parseEnumValues(content, "EntityQueryPacketVersion");
            auto avatarQuery = parseEnumValues(content, "AvatarQueryVersion");
            auto domAdded = parseEnumValues(content, "DomainServerAddedNodeVersion");
            auto domList = parseEnumValues(content, "DomainListVersion");
            auto domListReq = parseEnumValues(content, "DomainListRequestVersion");
            auto domDenied = parseEnumValues(content, "DomainConnectionDeniedVersion");
            auto ping = parseEnumValues(content, "PingVersion");
            auto icePing = parseEnumValues(content, "IcePingVersion");
            auto audio = parseEnumValues(content, "AudioVersion");

            s_vAvatarRemoveAttachments = static_cast<uint8_t>(avatar["RemoveAttachments"]);
            s_vAvatarTraitsAck = static_cast<uint8_t>(avatar["AvatarTraitsAck"]);
            s_vAvatarQueryConical = static_cast<uint8_t>(avatarQuery["ConicalFrustums"]);
            // Entity LAST_PACKET_TYPE is number of entries - 1 before NUM_PACKET_TYPE
            // If parsing map failed to give LAST_PACKET_TYPE, derive from count of entries before NUM_PACKET_TYPE label.
            int entityCount = 0;
            {
                // Count entries until NUM_PACKET_TYPE in the EntityVersion enum body
                auto ep = content.find("enum class EntityVersion");
                if (ep != std::string::npos) {
                    auto eb = content.find('{', ep);
                    auto ee = content.find("NUM_PACKET_TYPE", eb);
                    if (eb != std::string::npos && ee != std::string::npos) {
                        std::string body = content.substr(eb + 1, ee - eb - 1);
                        std::istringstream ls(body);
                        std::string l;
                        while (std::getline(ls, l)) {
                            auto cpos = l.find("//"); if (cpos != std::string::npos) l = l.substr(0, cpos);
                            auto notspace = [](int ch){ return !std::isspace(ch); };
                            l.erase(l.begin(), std::find_if(l.begin(), l.end(), notspace));
                            l.erase(std::find_if(l.rbegin(), l.rend(), notspace).base(), l.end());
                            if (l.empty()) continue;
                            if (l.find(',') != std::string::npos) entityCount++;
                        }
                    }
                }
            }
            s_vEntityLastPacket = entityCount > 0 ? static_cast<uint8_t>(entityCount - 1) : 23;
            s_vEntityParticleSpin = static_cast<uint8_t>(entity["ParticleSpin"]);
            s_vAssetBakingTextureMeta = static_cast<uint8_t>(asset["BakingTextureMeta"]);
            s_vEntityScriptClientCallable = static_cast<uint8_t>(entScript["ClientCallable"]);
            s_vEntityQueryCbor = static_cast<uint8_t>(entQuery["CborData"]);
            s_vDomainServerAddedNodeSocketTypes = static_cast<uint8_t>(domAdded["SocketTypes"]);
            s_vDomainListSocketTypes = static_cast<uint8_t>(domList["SocketTypes"]);
            s_vDomainListRequestSocketTypes = static_cast<uint8_t>(domListReq["SocketTypes"]);
            s_vDomainConnectionDeniedExtraInfo = static_cast<uint8_t>(domDenied["IncludesExtraInfo"]);
            s_vPingIncludeConnID = static_cast<uint8_t>(ping["IncludeConnectionID"]);
            s_vIcePingSendPeerID = static_cast<uint8_t>(icePing["SendICEPeerID"]);
            s_vAudioStopInjectors = static_cast<uint8_t>(audio["StopInjectors"]);
            s_numPacketTypes = parsePacketTypeCount(content);
            inited = true;
        } else {
            // Fallback values (best-known)
            s_vAvatarRemoveAttachments = 38; // conservative guess
            s_vAvatarTraitsAck = 43; // guess
            s_vEntityLastPacket = 99; // guess
            s_vAssetBakingTextureMeta = 22;
            s_vEntityScriptClientCallable = 19;
            s_vEntityQueryCbor = 24;
            s_vDomainServerAddedNodeSocketTypes = 19;
            s_vDomainListSocketTypes = 25;
            s_vDomainListRequestSocketTypes = 23;
            s_vDomainConnectionDeniedExtraInfo = 19;
            s_vPingIncludeConnID = 18;
            s_vIcePingSendPeerID = 18;
            s_vAudioStopInjectors = 24;
            s_numPacketTypes = 106;
            inited = true;
        }
    }
    vAvatarRemoveAttachments = s_vAvatarRemoveAttachments;
    vAvatarTraitsAck = s_vAvatarTraitsAck;
    vEntityLastPacket = s_vEntityLastPacket;
    vEntityParticleSpin = s_vEntityParticleSpin;
    vAssetBakingTextureMeta = s_vAssetBakingTextureMeta;
    vEntityScriptClientCallable = s_vEntityScriptClientCallable;
    vEntityQueryCbor = s_vEntityQueryCbor;
    vAvatarQueryConical = s_vAvatarQueryConical;
    vDomainServerAddedNodeSocketTypes = s_vDomainServerAddedNodeSocketTypes;
    vDomainListSocketTypes = s_vDomainListSocketTypes;
    vDomainListRequestSocketTypes = s_vDomainListRequestSocketTypes;
    vDomainConnectionDeniedExtraInfo = s_vDomainConnectionDeniedExtraInfo;
    vPingIncludeConnID = s_vPingIncludeConnID;
    vIcePingSendPeerID = s_vIcePingSendPeerID;
    vAudioStopInjectors = s_vAudioStopInjectors;
    numPacketTypes = s_numPacketTypes;
}

} // anonymous namespace

uint8_t NLPacket::versionForPacketType(PacketType type) {
    uint8_t vAvatarRemoveAttachments, vAvatarTraitsAck, vEntityLastPacket,
            vEntityParticleSpin, vAssetBakingTextureMeta, vEntityScriptClientCallable, vEntityQueryCbor,
            vAvatarQueryConical, vDomainServerAddedNodeSocketTypes, vDomainListSocketTypes, vDomainListRequestSocketTypes,
            vDomainConnectionDeniedExtraInfo, vPingIncludeConnID, vIcePingSendPeerID, vAudioStopInjectors;
    int numPacketTypes = 106;
    ensureVersionTable(vAvatarRemoveAttachments, vAvatarTraitsAck, vEntityLastPacket,
                       vEntityParticleSpin, vAssetBakingTextureMeta, vEntityScriptClientCallable, vEntityQueryCbor,
                       vAvatarQueryConical, vDomainServerAddedNodeSocketTypes, vDomainListSocketTypes, vDomainListRequestSocketTypes,
                       vDomainConnectionDeniedExtraInfo, vPingIncludeConnID, vIcePingSendPeerID, vAudioStopInjectors,
                       numPacketTypes);
    // Based on Overte's PacketHeaders.cpp versionForPacketType()
    // Returns the protocol version for each packet type
    switch (type) {
        case PacketType::DomainConnectRequest:
            return PacketVersions::DomainConnectRequest_SocketTypes;
        case PacketType::DomainListRequest:
            return vDomainListRequestSocketTypes;
        case PacketType::DomainList:
            return vDomainListSocketTypes;
        case PacketType::Ping:
            return vPingIncludeConnID;
        case PacketType::DomainConnectionDenied:
            return vDomainConnectionDeniedExtraInfo;
        case PacketType::DomainConnectRequestPending:
            return 17;
        case PacketType::PingReply:
            return 17;
        case PacketType::ICEServerPeerInformation:
        case PacketType::ICEServerQuery:
            return 17;
        case PacketType::ICEServerHeartbeat:
            return 18; // ICE server heartbeat signing
        case PacketType::ICEServerHeartbeatACK:
            return 17;
        case PacketType::ICEServerHeartbeatDenied:
            return 17;
        case PacketType::ICEPing:
            return vIcePingSendPeerID;
        case PacketType::ICEPingReply:
            return 17;
        case PacketType::NodeIgnoreRequest:
            return 18;
        case PacketType::DomainServerAddedNode:
            return vDomainServerAddedNodeSocketTypes;
        case PacketType::EntityAdd:
        case PacketType::EntityClone:
        case PacketType::EntityEdit:
        case PacketType::EntityData:
        case PacketType::EntityPhysics:
            return vEntityLastPacket;
        case PacketType::EntityQuery:
            return vEntityQueryCbor;
        case PacketType::EntityQueryInitialResultsComplete:
            return vEntityParticleSpin;
        case PacketType::AvatarQuery:
            return vAvatarQueryConical;
        case PacketType::AvatarIdentity:
        case PacketType::AvatarData:
        case PacketType::BulkAvatarData:
        case PacketType::KillAvatar:
            return vAvatarRemoveAttachments;
        case PacketType::BulkAvatarTraitsAck:
        case PacketType::BulkAvatarTraits:
            return vAvatarTraitsAck;
        case PacketType::MessagesData:
            return 18; // TextOrBinaryData
        case PacketType::AssetMappingOperation:
        case PacketType::AssetMappingOperationReply:
        case PacketType::AssetGetInfo:
        case PacketType::AssetGet:
        case PacketType::AssetUpload:
            return vAssetBakingTextureMeta;
        case PacketType::EntityScriptCallMethod:
            return vEntityScriptClientCallable;
        case PacketType::DomainSettings:
            return 18;
        case PacketType::MixedAudio:
        case PacketType::SilentAudioFrame:
        case PacketType::InjectAudio:
        case PacketType::MicrophoneAudioNoEcho:
        case PacketType::MicrophoneAudioWithEcho:
        case PacketType::AudioStreamStats:
        case PacketType::StopInjector:
            return vAudioStopInjectors;
        // For other packet types, return a default version
        // In real Overte, each has a specific version
            default:
                return 22;  // Default version for unspecified packets (matches Overte PacketHeaders.cpp)
    }
}

std::vector<uint8_t> NLPacket::computeProtocolVersionSignature() {
    // Protocol signature computed from Overte 2025.05.1 source (commit 53d2094)
    // Matches overte-server-bin AUR package
    // Computed via tools/compute_protocol_v2.py with actual enum parsing
    // Protocol version: "UuQR8qg5dU9NE9CXz2rEaQ==" (base64) = 52e411f2a839754f4d13d097cf6ac469 (hex)
    // Based on: 106 packet types, EntityVersion::LAST = 70, DomainListVersion::SocketTypes = 7
    // AvatarMixerPacketVersion::RemoveAttachments = 38, EntityQueryPacketVersion::ConicalFrustums = 23
    
    std::vector<uint8_t> signature = {
        0x52, 0xe4, 0x11, 0xf2, 0xa8, 0x39, 0x75, 0x4f,
        0x4d, 0x13, 0xd0, 0x97, 0xcf, 0x6a, 0xc4, 0x69
    };
    
    return signature;
}

} // namespace Overte
