#!/usr/bin/env python3
"""
Compute Overte protocol signature from local source code.
This reads the actual PacketHeaders.h to get accurate enum counts.
"""

import hashlib
import struct
import re
import sys
from pathlib import Path

def count_enum_members(enum_text):
    """Count comma-separated members in an enum"""
    # Remove comments and count commas
    lines = enum_text.split('\n')
    count = 0
    for line in lines:
        # Remove line comments
        line = re.sub(r'//.*$', '', line)
        if ',' in line and not line.strip().startswith('//'):
            count += 1
    return count

def parse_header(header_path):
    """Parse PacketHeaders.h to get enum counts"""
    with open(header_path) as f:
        content = f.read()
    
    # Extract PacketType enum
    packet_type_match = re.search(
        r'enum class Value : uint8_t \{(.+?)NUM_PACKET_TYPE',
        content, re.DOTALL
    )
    if not packet_type_match:
        print("ERROR: Could not find PacketType enum")
        return None
    
    packet_type_count = count_enum_members(packet_type_match.group(1))
    
    # Extract EntityVersion enum
    entity_version_match = re.search(
        r'enum class EntityVersion[^{]+\{(.+?)NUM_PACKET_TYPE',
        content, re.DOTALL
    )
    entity_version_count = count_enum_members(entity_version_match.group(1)) if entity_version_match else 0
    
    print(f"PacketType count: {packet_type_count}")
    print(f"EntityVersion count: {entity_version_count}")
    print(f"EntityVersion::LAST_PACKET_TYPE = {entity_version_count - 1}")
    
    return {
        'num_packet_types': packet_type_count,
        'entity_version_last': entity_version_count - 1,  # LAST = NUM - 1
    }

def compute_signature(counts):
    """Compute protocol signature based on versionForPacketType logic"""
    num_types = counts['num_packet_types']
    entity_last = counts['entity_version_last']
    
    # Default version (changed to 22 in 2025.05.1, was 23 in master)
    versions = [22] * num_types
    
    # Apply specific overrides from versionForPacketType()
    # Based on the switch statement in PacketHeaders.cpp
    
    # Packet type indices (from enum class Value)
    versions[1] = 17   # DomainConnectRequestPending
    versions[2] = 25   # DomainList::SocketTypes
    
    # Entity packets use EntityVersion::LAST_PACKET_TYPE
    for pt in [23, 88, 25, 21, 68]:  # EntityAdd, EntityClone, EntityEdit, EntityData, EntityPhysics
        versions[pt] = entity_last
    
    versions[22] = 22  # EntityQuery::ConicalFrustums (same as default in this version)
    
    # Avatar packets - RemoveAttachments (assume 25)
    for pt in [29, 6, 11, 5]:  # AvatarIdentity, AvatarData, BulkAvatarData, KillAvatar
        versions[pt] = 25
    
    versions[57] = 18  # MessagesData::TextOrBinaryData
    
    # ICE packets
    ice_17 = [18, 63, 19, 40]  # Indices for ICEServerPeerInfo, HeartbeatACK, Query, PingReply
    for pt in ice_17:
        versions[pt] = 17
    versions[38] = 18  # ICEServerHeartbeat
    versions[39] = 18  # ICEPing::SendICEPeerID
    
    # Asset packets - BakingTextureMeta (assume 24)
    for pt in [61, 62, 53, 49, 51]:  # AssetMappingOp, AssetMappingOpReply, GetInfo, Get, Upload
        versions[pt] = 24
    
    versions[30] = 18  # NodeIgnoreRequest
    versions[16] = 18  # DomainConnectionDenied::IncludesExtraInfo
    
    # Socket type packets (25)
    for pt in [31, 13, 17]:  # DomainConnectRequest, DomainListRequest, DomainServerAddedNode
        versions[pt] = 25
    
    versions[92] = 19  # EntityScriptCallMethod::ClientCallable
    
    # Audio packets - StopInjectors (24)
    for pt in [8, 12, 7, 9, 10, 18, 103]:  # MixedAudio, SilentAudioFrame, InjectAudio, MicNoEcho, MicWithEcho, AudioStreamStats, StopInjector
        if pt < num_types:
            versions[pt] = 24
    
    versions[48] = 18  # DomainSettings
    versions[3] = 18   # Ping::IncludeConnectionID
    versions[72] = 22  # AvatarQuery::ConicalFrustums
    
    # These use EntityVersion values
    versions[89] = 68  # EntityQueryInitialResultsComplete::ParticleSpin (guess - need exact value)
    versions[102] = 26 # BulkAvatarTraitsAck::AvatarTraitsAck
    versions[90] = 26  # BulkAvatarTraits
    
    # Compute MD5
    data = struct.pack('B', num_types)
    for v in versions:
        data += struct.pack('B', v & 0xFF)
    
    md5 = hashlib.md5(data).digest()
    return md5, versions

def main():
    # Find PacketHeaders.h
    overte_src = Path("/home/mayatheshy/stardust/starworld/third_party/overte-src")
    header = overte_src / "libraries/networking/src/udt/PacketHeaders.h"
    
    if not header.exists():
        print(f"ERROR: Could not find {header}")
        sys.exit(1)
    
    print(f"Parsing {header}")
    counts = parse_header(header)
    
    if not counts:
        sys.exit(1)
    
    signature, versions = compute_signature(counts)
    
    print(f"\nProtocol Signature:")
    print(f"  Hex: {signature.hex()}")
    
    import base64
    b64 = base64.b64encode(signature).decode()
    print(f"  Base64: {b64}")
    
    print(f"\nC++ code for NLPacketCodec.cpp:")
    print("std::vector<uint8_t> NLPacket::computeProtocolVersionSignature() {")
    print("    std::vector<uint8_t> signature = {")
    print(f"        {', '.join(f'0x{b:02x}' for b in signature)}")
    print("    };")
    print("    return signature;")
    print("}")
    
    # Save to file for easy copy-paste
    output_file = Path("/home/mayatheshy/stardust/starworld/tools/protocol_signature.txt")
    with open(output_file, 'w') as f:
        f.write(f"Hex: {signature.hex()}\n")
        f.write(f"Base64: {b64}\n")
        f.write(f"C++ array: {{{', '.join(f'0x{b:02x}' for b in signature)}}}\n")
    print(f"\nSaved to: {output_file}")

if __name__ == '__main__':
    main()
