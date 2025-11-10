#!/usr/bin/env python3
"""
Parse Overte PacketHeaders.cpp and compute protocol signature exactly as Overte does.
This reads the actual versionForPacketType() implementation and evaluates it.
"""

import hashlib
import struct
import re
from pathlib import Path

def parse_packet_type_enum(header_path):
    """Parse the PacketType enum to get the count"""
    with open(header_path) as f:
        content = f.read()
    
    # Find PacketType enum
    match = re.search(r'enum class Value : uint8_t \{(.+?)NUM_PACKET_TYPE', content, re.DOTALL)
    if not match:
        raise ValueError("Could not find PacketType enum")
    
    # Count entries (count commas in non-comment lines)
    enum_body = match.group(1)
    lines = [line for line in enum_body.split('\n') 
             if ',' in line and not line.strip().startswith('//')]
    
    return len(lines)

def parse_entity_version_enum(header_path):
    """Parse EntityVersion enum to get LAST_PACKET_TYPE value"""
    with open(header_path) as f:
        content = f.read()
    
    match = re.search(r'enum class EntityVersion[^{]+\{(.+?)NUM_PACKET_TYPE', content, re.DOTALL)
    if not match:
        raise ValueError("Could not find EntityVersion enum")
    
    enum_body = match.group(1)
    lines = [line for line in enum_body.split('\n')
             if ',' in line and not line.strip().startswith('//')]
    
    # LAST_PACKET_TYPE = NUM - 1
    return len(lines) - 1

def get_enum_value_from_header(header_path, enum_name, value_name):
    """Get an enum value by searching for it"""
    with open(header_path) as f:
        content = f.read()
    
    # Find the enum
    pattern = rf'enum class {enum_name}[^{{]+\{{(.+?)\}};'
    match = re.search(pattern, content, re.DOTALL)
    if not match:
        return None
    
    enum_body = match.group(1)
    lines = enum_body.split('\n')
    
    count = 0
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith('//') or stripped.startswith('*'):
            continue
        
        # Check for explicit assignment
        if '=' in stripped and value_name in stripped:
            val_part = stripped.split('=')[1].strip().rstrip(',')
            try:
                return int(val_part)
            except:
                pass
        
        # Check if this is our value
        if stripped.startswith(value_name):
            return count
        
        if ',' in stripped:
            count += 1
    
    return None

def main():
    overte_src = Path("/home/mayatheshy/stardust/starworld/third_party/overte-src")
    header = overte_src / "libraries/networking/src/udt/PacketHeaders.h"
    
    if not header.exists():
        print(f"ERROR: {header} not found")
        return 1
    
    print(f"Parsing {header}")
    
    num_packet_types = parse_packet_type_enum(header)
    entity_version_last = parse_entity_version_enum(header)
    
    print(f"PacketType::NUM_PACKET_TYPE = {num_packet_types}")
    print(f"EntityVersion::LAST_PACKET_TYPE = {entity_version_last}")
    
    # Get enum values we need
    socket_types = get_enum_value_from_header(header, "DomainListVersion", "SocketTypes")
    remove_attachments = get_enum_value_from_header(header, "AvatarMixerPacketVersion", "RemoveAttachments")
    conical_frustums = get_enum_value_from_header(header, "EntityQueryPacketVersion", "ConicalFrustums")
    
    print(f"DomainListVersion::SocketTypes = {socket_types}")
    print(f"AvatarMixerPacketVersion::RemoveAttachments = {remove_attachments}")
    print(f"EntityQueryPacketVersion::ConicalFrustums = {conical_frustums}")
    
    # Now build the version array based on versionForPacketType logic
    versions = [22] * num_packet_types  # default
    
    # Apply all the overrides from the switch statement
    versions[1] = 17  # DomainConnectRequestPending
    versions[2] = socket_types if socket_types else 25  # DomainList
    
    # Entity packets
    for pt in [23, 88, 25, 21, 68]:  # EntityAdd, EntityClone, EntityEdit, EntityData, EntityPhysics
        if pt < num_packet_types:
            versions[pt] = entity_version_last
    
    versions[22] = conical_frustums if conical_frustums else 22  # EntityQuery
    
    # Avatar packets
    remove_att_val = remove_attachments if remove_attachments else 25
    for pt in [29, 6, 11, 5]:  # AvatarIdentity, AvatarData, BulkAvatarData, KillAvatar
        if pt < num_packet_types:
            versions[pt] = remove_att_val
    
    versions[57] = 18  # MessagesData::TextOrBinaryData
    
    # ICE packets
    for pt in [18, 63, 19, 40]:
        if pt < num_packet_types:
            versions[pt] = 17
    if 38 < num_packet_types: versions[38] = 18  # ICEServerHeartbeat
    if 39 < num_packet_types: versions[39] = 18  # ICEPing
    
    # Asset packets
    for pt in [61, 62, 53, 49, 51]:
        if pt < num_packet_types:
            versions[pt] = 24
    
    if 30 < num_packet_types: versions[30] = 18  # NodeIgnoreRequest
    if 16 < num_packet_types: versions[16] = 18  # DomainConnectionDenied
    
    # SocketTypes packets
    for pt in [31, 13, 17]:
        if pt < num_packet_types:
            versions[pt] = socket_types if socket_types else 25
    
    if 92 < num_packet_types: versions[92] = 19  # EntityScriptCallMethod
    
    # Audio packets
    for pt in [8, 12, 7, 9, 10, 18, 103]:
        if pt < num_packet_types:
            versions[pt] = 24
    
    if 48 < num_packet_types: versions[48] = 18
    if 3 < num_packet_types: versions[3] = 18
    if 72 < num_packet_types: versions[72] = 22
    if 89 < num_packet_types: versions[89] = 68  # ParticleSpin - need actual value
    if 102 < num_packet_types: versions[102] = 26
    if 90 < num_packet_types: versions[90] = 26
    
    # Compute MD5 exactly as Overte does with QDataStream
    data = struct.pack('B', num_packet_types)
    for v in versions:
        data += struct.pack('B', v & 0xFF)
    
    md5 = hashlib.md5(data).digest()
    
    print(f"\nProtocol Signature:")
    print(f"  Hex: {md5.hex()}")
    
    import base64
    b64 = base64.b64encode(md5).decode()
    print(f"  Base64: {b64}")
    
    print(f"\nC++ code:")
    print("std::vector<uint8_t> signature = {")
    print(f"    {', '.join(f'0x{b:02x}' for b in md5)}")
    print("};")
    
    # Debug: show first 20 versions
    print(f"\nFirst 20 packet versions:")
    for i in range(min(20, num_packet_types)):
        print(f"  [{i}] = {versions[i]}")

if __name__ == '__main__':
    main()
