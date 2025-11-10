// Compile and run this to get the exact protocol signature from Overte source
// Compile: cd /home/mayatheshy/stardust/starworld/third_party/overte-src && \
//   g++ -o /tmp/compute_protocol -I libraries/networking/src/udt \
//   -I /usr/include/qt5 -I /usr/include/qt5/QtCore \
//   /home/mayatheshy/stardust/starworld/tools/compute_from_overte.cpp \
//   -lQt5Core -std=c++17 -fPIC

#include <QCoreApplication>
#include <QByteArray>
#include <QDataStream>
#include <QCryptographicHash>
#include <QDebug>
#include <iostream>
#include <cstdint>

// Copy the enum definitions and versionForPacketType from Overte
#include "../../../third_party/overte-src/libraries/networking/src/udt/PacketHeaders.h"

// We need to include the actual implementation
// Since we can't easily link against Overte, let's just compute it inline

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // Compute the protocol signature the same way Overte does
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    
    uint8_t numberOfProtocols = static_cast<uint8_t>(PacketType::NUM_PACKET_TYPE);
    stream << numberOfProtocols;
    
    std::cout << "Number of packet types: " << (int)numberOfProtocols << std::endl;
    std::cout << "Protocol versions:" << std::endl;
    
    for (uint8_t packetType = 0; packetType < numberOfProtocols; packetType++) {
        uint8_t packetTypeVersion = static_cast<uint8_t>(versionForPacketType(static_cast<PacketType>(packetType)));
        stream << packetTypeVersion;
        
        if (packetType < 20 || packetTypeVersion != 22) {
            std::cout << "  [" << (int)packetType << "] = " << (int)packetTypeVersion << std::endl;
        }
    }
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(buffer);
    QByteArray signature = hash.result();
    
    std::cout << "\nProtocol Signature:" << std::endl;
    std::cout << "  Hex: " << signature.toHex().toStdString() << std::endl;
    std::cout << "  Base64: " << signature.toBase64().toStdString() << std::endl;
    
    std::cout << "\nC++ array:" << std::endl;
    std::cout << "std::vector<uint8_t> signature = {" << std::endl;
    std::cout << "    ";
    for (int i = 0; i < signature.size(); i++) {
        printf("0x%02x", (unsigned char)signature[i]);
        if (i < signature.size() - 1) std::cout << ", ";
    }
    std::cout << "\n};" << std::endl;
    
    return 0;
}
