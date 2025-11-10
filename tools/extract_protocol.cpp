// Extract protocol signature from Overte's libnetworking.so
// Compile: g++ -o extract_protocol extract_protocol.cpp -L/opt/overte/lib -lnetworking -Wl,-rpath,/opt/overte/lib
// Run: LD_LIBRARY_PATH=/opt/overte/lib:$LD_LIBRARY_PATH ./extract_protocol

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>

// Declare the external C++ mangled function from libnetworking.so
// Symbol: _Z31protocolVersionsSignatureBase64v
extern "C" {
    // We don't know the exact return type, but it's likely a QString or similar
    // Let's try to call the hex version instead
    // Symbol: _Z28protocolVersionsSignatureHexv
    void* _Z28protocolVersionsSignatureHexv();
    void* _Z31protocolVersionsSignatureBase64v();
    void* _Z25protocolVersionsSignaturev();
}

int main() {
    std::cout << "Attempting to extract protocol signature from /opt/overte/lib/libnetworking.so" << std::endl;
    
    try {
        // Call the mangled function
        void* result = _Z31protocolVersionsSignatureBase64v();
        
        // Qt QString structure (simplified - may not work):
        // Assuming QString is returned by value or we get a pointer
        // This is tricky without Qt headers...
        
        std::cout << "Got result pointer: " << result << std::endl;
        
        // Try the raw bytes version instead
        void* raw_result = _Z25protocolVersionsSignaturev();
        std::cout << "Got raw result pointer: " << raw_result << std::endl;
        
        // Without Qt, this won't work properly
        std::cout << "ERROR: This approach requires linking against Qt and including Qt headers" << std::endl;
        std::cout << "Try using 'strings' command instead:" << std::endl;
        std::cout << "  strings /opt/overte/domain-server | grep -E '^[A-Za-z0-9+/]{22}==$'" << std::endl;
        
    } catch (...) {
        std::cout << "Exception caught" << std::endl;
    }
    
    return 0;
}
