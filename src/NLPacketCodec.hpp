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
    Unknown,
    DomainConnectRequestPending,
    DomainList,
    Ping,
    PingReply,
    KillAvatar,
    AvatarData,
    InjectAudio,
    MixedAudio,
    MicrophoneAudioNoEcho,
    MicrophoneAudioWithEcho,
    BulkAvatarData,
    SilentAudioFrame,
    DomainListRequest,
    RequestAssignment,
    CreateAssignment,
    DomainConnectionDenied,
    MuteEnvironment,
    AudioStreamStats,
    DomainServerPathQuery,
    DomainServerPathResponse,
    DomainServerAddedNode,
    ICEServerPeerInformation,
    ICEServerQuery,
    OctreeStats,
    SetAvatarTraits,
    InjectorGainSet,
    AssignmentClientStatus,
    NoisyMute,
    AvatarIdentity,
    NodeIgnoreRequest,
    DomainConnectRequest,
    DomainServerRequireDTLS,
    NodeJsonStats,
    OctreeDataNack,
    StopNode,
    AudioEnvironment,
    EntityEditNack,
    ICEServerHeartbeat,
    ICEPing,
    ICEPingReply,
    EntityData,
    EntityQuery,
    EntityAdd,
    EntityErase,
    EntityEdit,
    DomainServerConnectionToken,
    DomainSettingsRequest,
    DomainSettings,
    AssetGet,
    AssetGetReply,
    AssetUpload,
    AssetUploadReply,
    AssetGetInfo,
    AssetGetInfoReply,
    DomainDisconnectRequest,
    DomainServerRemovedNode,
    MessagesData,
    MessagesSubscribe,
    MessagesUnsubscribe,
    ICEServerHeartbeatDenied,
    AssetMappingOperation,
    AssetMappingOperationReply,
    ICEServerHeartbeatACK,
    NegotiateAudioFormat,
    SelectedAudioFormat,
    MoreEntityShapes,
    NodeKickRequest,
    NodeMuteRequest,
    RadiusIgnoreRequest,
    UsernameFromIDRequest,
    UsernameFromIDReply,
    AvatarQuery,
    RequestsDomainListData,
    PerAvatarGainSet,
    EntityScriptGetStatus,
    EntityScriptGetStatusReply,
    ReloadEntityServerScript,
    EntityPhysics,
    EntityServerScriptLog,
    AdjustAvatarSorting,
    OctreeFileReplacement,
    CollisionEventChanges,
    ReplicatedMicrophoneAudioNoEcho,
    ReplicatedMicrophoneAudioWithEcho,
    ReplicatedInjectAudio,
    ReplicatedSilentAudioFrame,
    ReplicatedAvatarIdentity,
    ReplicatedKillAvatar,
    ReplicatedBulkAvatarData,
    DomainContentReplacementFromUrl,
    DropOnNextProtocolChange_1,
    EntityScriptCallMethod,
    DropOnNextProtocolChange_2,
    DropOnNextProtocolChange_3,
    OctreeDataFileRequest,
    OctreeDataFileReply,
    OctreeDataPersist,
    EntityClone,
    EntityQueryInitialResultsComplete,
    BulkAvatarTraits,
    AudioSoloRequest,
    BulkAvatarTraitsAck,
    StopInjector,
    AvatarZonePresence,
    WebRTCSignaling,
    NUM_PACKET_TYPE
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
    
    // Write HMAC-MD5 verification hash
    void writeVerificationHash(const uint8_t* connectionSecretUUID);
    
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
