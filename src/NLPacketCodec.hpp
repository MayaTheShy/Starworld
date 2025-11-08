// NLPacketCodec.hpp
// Minimal NLPacket protocol implementation for Overte domain communication
// Based on Overte's NLPacket.h specification

#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <string>

namespace Overte {

// Packet types from Overte protocol
enum class PacketType : uint8_t {
    Unknown = 0,
    DomainConnectRequestPending = 1,
    DomainList = 2,
    Ping = 3,
    PingReply = 4,
    KillAvatar = 5,
    AvatarData = 6,
    InjectAudio = 7,
    MixedAudio = 8,
    MicrophoneAudioNoEcho = 9,
    MicrophoneAudioWithEcho = 10,
    BulkAvatarData = 11,
    SilentAudioFrame = 12,
    DomainListRequest = 13,
    RequestAssignment = 14,
    CreateAssignment = 15,
    DomainConnectionDenied = 16,
    MuteEnvironment = 17,
    AudioStreamStats = 18,
    DomainServerPathQuery = 19,
    DomainServerPathResponse = 20,
    DomainServerAddedNode = 21,
    ICEServerPeerInformation = 22,
    ICEServerQuery = 23,
    OctreeStats = 24,
    SetAvatarTraits = 25,
    InjectorGainSet = 26,
    AssignmentClientStatus = 27,
    NoisyMute = 28,
    AvatarIdentity = 29,
    NodeIgnoreRequest = 30,
    DomainConnectRequest = 31,
    DomainServerRequireDTLS = 32,
    // ... many more packet types
    EntityAdd = 0x41,
    EntityEdit = 0x42,
    EntityErase = 0x43,
    EntityQuery = 0x44,
    EntityData = 0x45,
};

using PacketVersion = uint8_t;
using LocalID = uint16_t;
using SequenceNumber = uint32_t;

// Packet version constants (from Overte source)
namespace PacketVersions {
    constexpr PacketVersion DomainConnectRequest_SocketTypes = 27;  // NoHostname(17) + 10
    constexpr PacketVersion DomainListRequest_SocketTypes = 23;     // PreSocketTypes(22) + 1
    constexpr PacketVersion DomainList_SocketTypes = 25;            // PrePermissionsGrid(18) + 7
    constexpr PacketVersion Ping_IncludeConnectionID = 18;
}

// NLPacket structure (minimal implementation)
class NLPacket {
public:
    // Packet header components
    struct Header {
        uint32_t sequenceAndFlags;  // Sequence number (29 bits) + flags (3 bits)
        PacketType type;
        PacketVersion version;
        LocalID sourceID;  // Only for sourced packets
    };
    
    static constexpr LocalID NULL_LOCAL_ID = 0;
    static constexpr size_t BASE_HEADER_SIZE = sizeof(uint32_t) + sizeof(PacketType) + sizeof(PacketVersion);
    static constexpr size_t SOURCED_HEADER_SIZE = BASE_HEADER_SIZE + sizeof(LocalID);
    
    NLPacket(PacketType type, PacketVersion version = 0, bool isReliable = false);
    
    // Write data to packet payload
    void write(const void* data, size_t size);
    void writeUInt8(uint8_t value);
    void writeUInt16(uint16_t value);
    void writeUInt32(uint32_t value);
    void writeUInt64(uint64_t value);
    void writeString(const std::string& str, bool nullTerminated = true);
    
    // Get packet data for sending
    const std::vector<uint8_t>& getData() const { return m_data; }
    size_t getSize() const { return m_data.size(); }
    
    // Parse received packet
    static bool parseHeader(const uint8_t* data, size_t size, Header& header);
    static PacketType getType(const uint8_t* data, size_t size);
    
    void setSequenceNumber(SequenceNumber seq);
    void setSourceID(LocalID id);
    
    // Protocol version signature
    static std::vector<uint8_t> computeProtocolVersionSignature();
    static uint8_t versionForPacketType(PacketType type);
    
private:
    void writeHeader();
    
    PacketType m_type;
    PacketVersion m_version;
    SequenceNumber m_sequenceNumber{0};
    LocalID m_sourceID{NULL_LOCAL_ID};
    bool m_isReliable;
    bool m_isSourced{false};
    
    std::vector<uint8_t> m_data;
    size_t m_headerSize;
};

} // namespace Overte
