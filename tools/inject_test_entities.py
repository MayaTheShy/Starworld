#!/usr/bin/env python3
"""
Simple script to send test entity packets to the OverteClient.
This simulates what the EntityServer would send.
"""

import socket
import struct
import time

# Overte packet types (as defined in your C++ code)
PACKET_TYPE_ENTITY_ADD = 0x10
PACKET_TYPE_ENTITY_EDIT = 0x11
PACKET_TYPE_ENTITY_ERASE = 0x12

def send_entity_add(sock, addr, entity_id, name):
    """Send an EntityAdd packet"""
    # Packet structure: [type:u8][id:u64][name:null-terminated string]
    packet = struct.pack('<BQ', PACKET_TYPE_ENTITY_ADD, entity_id)
    packet += name.encode('utf-8') + b'\x00'
    
    sock.sendto(packet, addr)
    print(f"Sent EntityAdd: id={entity_id}, name={name}")

def send_entity_edit(sock, addr, entity_id):
    """Send an EntityEdit packet (simplified)"""
    # Packet structure: [type:u8][id:u64][property flags...]
    packet = struct.pack('<BQ', PACKET_TYPE_ENTITY_EDIT, entity_id)
    
    sock.sendto(packet, addr)
    print(f"Sent EntityEdit: id={entity_id}")

def send_entity_erase(sock, addr, entity_id):
    """Send an EntityErase packet"""
    # Packet structure: [type:u8][id:u64]
    packet = struct.pack('<BQ', PACKET_TYPE_ENTITY_ERASE, entity_id)
    
    sock.sendto(packet, addr)
    print(f"Sent EntityErase: id={entity_id}")

def main():
    # Target: your OverteClient's EntityServer socket
    target_host = '127.0.0.1'
    target_port = 40103
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    addr = (target_host, target_port)
    
    print(f"Sending test entity packets to {target_host}:{target_port}")
    print("Make sure your stardust-overte-client is running!")
    print()
    
    time.sleep(1)
    
    # Create some test entities
    send_entity_add(sock, addr, 1001, "TestCube")
    time.sleep(0.5)
    
    send_entity_add(sock, addr, 1002, "TestSphere")
    time.sleep(0.5)
    
    send_entity_add(sock, addr, 1003, "TestBox")
    time.sleep(2)
    
    # Update an entity
    print("\nSending edit packets...")
    for i in range(3):
        send_entity_edit(sock, addr, 1001)
        time.sleep(0.5)
    
    time.sleep(2)
    
    # Delete an entity
    print("\nDeleting entity 1002...")
    send_entity_erase(sock, addr, 1002)
    
    print("\nDone! Check your client output.")
    sock.close()

if __name__ == '__main__':
    main()
