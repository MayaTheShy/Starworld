#!/usr/bin/env python3
"""
Enhanced entity injection script with full property support:
- Position (x, y, z)
- Rotation (quaternion: x, y, z, w)
- Dimensions (x, y, z) / Scale
- Model URL
- Texture URL
- Color
"""

import socket
import struct
import time
import argparse

# Overte packet types
PACKET_TYPE_ENTITY_ADD = 0x10
PACKET_TYPE_ENTITY_EDIT = 0x11
PACKET_TYPE_ENTITY_ERASE = 0x12

def send_entity_add_full(sock, addr, entity_id, name, position, rotation, dimensions, model_url="", texture_url="", color=(1.0, 1.0, 1.0)):
    """
    Send a full EntityAdd packet with all properties
    
    Args:
        entity_id: uint64
        name: string
        position: (x, y, z) tuple of floats
        rotation: (x, y, z, w) quaternion tuple of floats
        dimensions: (x, y, z) tuple of floats
        model_url: string (optional)
        texture_url: string (optional)
        color: (r, g, b) tuple of floats 0-1 (optional)
    """
    # Packet structure:
    # [type:u8][id:u64][name:null-terminated][position:3xf32][rotation:4xf32][dimensions:3xf32][model_url:null-terminated][texture_url:null-terminated][color:3xf32]
    
    packet = struct.pack('<BQ', PACKET_TYPE_ENTITY_ADD, entity_id)
    
    # Name (null-terminated string)
    packet += name.encode('utf-8') + b'\x00'
    
    # Position (vec3 - 3 floats)
    packet += struct.pack('<fff', position[0], position[1], position[2])
    
    # Rotation (quaternion - 4 floats: x, y, z, w)
    packet += struct.pack('<ffff', rotation[0], rotation[1], rotation[2], rotation[3])
    
    # Dimensions/Scale (vec3 - 3 floats)
    packet += struct.pack('<fff', dimensions[0], dimensions[1], dimensions[2])
    
    # Model URL (null-terminated string)
    packet += model_url.encode('utf-8') + b'\x00'
    
    # Texture URL (null-terminated string)
    packet += texture_url.encode('utf-8') + b'\x00'
    
    # Color (vec3 - 3 floats RGB 0-1)
    packet += struct.pack('<fff', color[0], color[1], color[2])
    
    sock.sendto(packet, addr)
    print(f"✓ Sent EntityAdd: {name} (id={entity_id})")
    print(f"  Position: ({position[0]:.2f}, {position[1]:.2f}, {position[2]:.2f})")
    print(f"  Rotation: ({rotation[0]:.2f}, {rotation[1]:.2f}, {rotation[2]:.2f}, {rotation[3]:.2f})")
    print(f"  Dimensions: ({dimensions[0]:.2f}, {dimensions[1]:.2f}, {dimensions[2]:.2f})")
    if model_url:
        print(f"  Model: {model_url}")
    if texture_url:
        print(f"  Texture: {texture_url}")
    print(f"  Color: RGB({color[0]:.2f}, {color[1]:.2f}, {color[2]:.2f})")

def send_entity_edit_transform(sock, addr, entity_id, position=None, rotation=None, dimensions=None):
    """
    Send an EntityEdit packet to update transform properties
    
    Args:
        entity_id: uint64
        position: (x, y, z) tuple or None
        rotation: (x, y, z, w) quaternion tuple or None  
        dimensions: (x, y, z) tuple or None
    """
    # Property flags bitfield
    HAS_POSITION = 0x01
    HAS_ROTATION = 0x02
    HAS_DIMENSIONS = 0x04
    
    flags = 0
    data = b''
    
    if position is not None:
        flags |= HAS_POSITION
        data += struct.pack('<fff', position[0], position[1], position[2])
    
    if rotation is not None:
        flags |= HAS_ROTATION
        data += struct.pack('<ffff', rotation[0], rotation[1], rotation[2], rotation[3])
    
    if dimensions is not None:
        flags |= HAS_DIMENSIONS
        data += struct.pack('<fff', dimensions[0], dimensions[1], dimensions[2])
    
    # Packet: [type:u8][id:u64][flags:u8][property data...]
    packet = struct.pack('<BQB', PACKET_TYPE_ENTITY_EDIT, entity_id, flags)
    packet += data
    
    sock.sendto(packet, addr)
    print(f"✓ Sent EntityEdit: id={entity_id}, flags=0x{flags:02x}")

def send_entity_erase(sock, addr, entity_id):
    """Send an EntityErase packet"""
    packet = struct.pack('<BQ', PACKET_TYPE_ENTITY_ERASE, entity_id)
    sock.sendto(packet, addr)
    print(f"✓ Sent EntityErase: id={entity_id}")

def demo_scene(sock, addr):
    """Create a demo scene with various entities"""
    
    print("\n=== Creating Demo Scene ===\n")
    
    # Red cube in front
    send_entity_add_full(
        sock, addr,
        entity_id=1001,
        name="RedCube",
        position=(0.0, 1.5, -2.0),
        rotation=(0.0, 0.0, 0.0, 1.0),  # Identity quaternion
        dimensions=(0.3, 0.3, 0.3),
        color=(1.0, 0.0, 0.0)  # Red
    )
    time.sleep(0.3)
    
    # Green sphere to the left
    send_entity_add_full(
        sock, addr,
        entity_id=1002,
        name="GreenSphere",
        position=(-1.0, 1.5, -2.5),
        rotation=(0.0, 0.0, 0.0, 1.0),
        dimensions=(0.4, 0.4, 0.4),
        color=(0.0, 1.0, 0.0)  # Green
    )
    time.sleep(0.3)
    
    # Blue box to the right
    send_entity_add_full(
        sock, addr,
        entity_id=1003,
        name="BlueBox",
        position=(1.0, 1.5, -2.5),
        rotation=(0.0, 0.0, 0.0, 1.0),
        dimensions=(0.5, 0.2, 0.3),
        color=(0.0, 0.0, 1.0)  # Blue
    )
    time.sleep(0.3)
    
    # Yellow tall box in back
    send_entity_add_full(
        sock, addr,
        entity_id=1004,
        name="YellowPillar",
        position=(0.0, 1.5, -4.0),
        rotation=(0.0, 0.0, 0.0, 1.0),
        dimensions=(0.2, 0.8, 0.2),
        color=(1.0, 1.0, 0.0)  # Yellow
    )
    time.sleep(0.3)
    
    # Cyan rotating cube (45 degrees on Y axis)
    import math
    angle = math.radians(45)
    quat_y = (0.0, math.sin(angle/2), 0.0, math.cos(angle/2))
    
    send_entity_add_full(
        sock, addr,
        entity_id=1005,
        name="RotatedCube",
        position=(0.0, 1.0, -1.5),
        rotation=quat_y,
        dimensions=(0.25, 0.25, 0.25),
        color=(0.0, 1.0, 1.0)  # Cyan
    )
    
    print("\n✓ Scene created with 5 entities\n")

def animate_scene(sock, addr):
    """Animate the entities"""
    print("\n=== Animating Scene ===\n")
    
    import math
    
    for frame in range(60):
        t = frame / 10.0
        
        # Move red cube in a circle
        x = math.sin(t) * 0.5
        z = -2.0 + math.cos(t) * 0.5
        send_entity_edit_transform(
            sock, addr, 1001,
            position=(x, 1.5, z)
        )
        
        # Rotate green sphere
        angle = t
        quat = (0.0, math.sin(angle/2), 0.0, math.cos(angle/2))
        send_entity_edit_transform(
            sock, addr, 1002,
            rotation=quat
        )
        
        # Scale blue box up and down
        scale = 0.3 + abs(math.sin(t)) * 0.3
        send_entity_edit_transform(
            sock, addr, 1003,
            dimensions=(scale, 0.2, scale)
        )
        
        time.sleep(0.1)
    
    print("\n✓ Animation complete\n")

def cleanup_scene(sock, addr):
    """Remove all demo entities"""
    print("\n=== Cleaning Up Scene ===\n")
    
    for entity_id in [1001, 1002, 1003, 1004, 1005]:
        send_entity_erase(sock, addr, entity_id)
        time.sleep(0.2)
    
    print("\n✓ All entities removed\n")

def main():
    parser = argparse.ArgumentParser(description='Enhanced Overte entity injection tool')
    parser.add_argument('--host', default='127.0.0.1', help='Target host (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=40103, help='Target port (default: 40103)')
    parser.add_argument('--no-animate', action='store_true', help='Skip animation phase')
    parser.add_argument('--no-cleanup', action='store_true', help='Keep entities after demo')
    
    args = parser.parse_args()
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    addr = (args.host, args.port)
    
    print(f"\n{'='*60}")
    print(f"  Enhanced Entity Injection Tool")
    print(f"  Target: {args.host}:{args.port}")
    print(f"{'='*60}")
    print("\nMake sure stardust-overte-client is running!")
    print("Waiting 2 seconds before starting...\n")
    
    time.sleep(2)
    
    try:
        # Create demo scene
        demo_scene(sock, addr)
        
        if not args.no_animate:
            time.sleep(1)
            animate_scene(sock, addr)
        
        if not args.no_cleanup:
            time.sleep(1)
            cleanup_scene(sock, addr)
        else:
            print("\n✓ Scene left active (use --no-cleanup to keep entities)\n")
    
    except KeyboardInterrupt:
        print("\n\nInterrupted! Cleaning up...")
        cleanup_scene(sock, addr)
    
    finally:
        sock.close()
        print("Done!")

if __name__ == '__main__':
    main()
