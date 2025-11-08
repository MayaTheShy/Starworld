// NLPacketCodec.cpp
#include "NLPacketCodec.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <string>

namespace Overte {

// Control bit masks for sequence number field
static constexpr uint32_t CONTROL_BIT_MASK = 0x80000000;      // Bit 31
static constexpr uint32_t RELIABLE_BIT_MASK = 0x40000000;     // Bit 30  
static constexpr uint32_t MESSAGE_BIT_MASK = 0x20000000;      // Bit 29
static constexpr uint32_t OBFUSCATION_MASK = 0x18000000;      // Bits 27-28
static constexpr uint32_t SEQUENCE_NUMBER_MASK = 0x07FFFFFF;  // Bits 0-26

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

} // namespace Overte
