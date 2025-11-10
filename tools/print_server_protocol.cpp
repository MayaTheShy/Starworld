#include <iostream>
#include <iomanip>
#include "../third_party/overte-src/libraries/networking/src/udt/PacketHeaders.h"

int main() {
    QByteArray sig = protocolVersionsSignature();
    
    std::cout << "Server protocol signature (hex): ";
    for (int i = 0; i < sig.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)(unsigned char)sig[i];
    }
    std::cout << std::endl;
    
    std::cout << "Server protocol signature (base64): " 
              << protocolVersionsSignatureBase64().toStdString() << std::endl;
    
    return 0;
}
