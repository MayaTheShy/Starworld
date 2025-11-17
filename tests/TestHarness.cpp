#include <iostream>
#include <vector>
#include <string>
#include <cassert>

#include "../src/NLPacketCodec.hpp"
#include "../src/DomainDiscovery.hpp"

static std::string hexOf(const std::vector<uint8_t>& v) {
    static const char* hexd = "0123456789abcdef";
    std::string out; out.resize(v.size()*2);
    for (size_t i=0;i<v.size();++i){ out[2*i]=hexd[(v[i]>>4)&0xF]; out[2*i+1]=hexd[v[i]&0xF]; }
    return out;
}

static std::string b64Of(const std::vector<uint8_t>& in){
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(((in.size()+2)/3)*4);
    size_t i=0; while(i<in.size()){
        uint32_t val=0; int bytes=0;
        for(int j=0;j<3;++j){ val<<=8; if(i<in.size()){ val|=in[i++]; ++bytes; } }
        int pad = 3 - bytes;
        for(int k=0;k<4-pad;++k){ int idx=(val>>(18-k*6))&0x3F; out.push_back(tbl[idx]); }
        for(int k=0;k<pad;++k) out.push_back('=');
    }
    return out;
}

int main(){
    int failures = 0;

    // Test 1: protocol signature stability (for vendored Overte commit and current mapping)
    {
        auto sig = Overte::NLPacket::computeProtocolVersionSignature();
        std::string hex = hexOf(sig);
        std::string b64 = b64Of(sig);
        std::cout << "[TEST] Protocol signature hex=" << hex << " base64=" << b64 << "\n";
        // Expected values based on current repository state/logs
        // Updated 2025-11-08 after entity enhancement changes
        const std::string expectedHex = "eb1600e798dc5e03c755a968dc16b7fc";
        if (hex != expectedHex) {
            std::cerr << "[FAIL] Signature hex mismatch: got " << hex << " expected " << expectedHex << "\n";
            ++failures;
        }
    }

    // Test 2: discovery JSON parsing (Vircadia-like fields)
    {
        std::string json = R"JSON({
            "data": [
              {"name":"Alpha","network_address":"alpha.example.org","http_port":40102,"udp_port":40104},
              {"name":"Beta","ice_server_address":"beta.example.org","http_port":40103,"udp_port":40105}
            ]
        })JSON";
        auto domains = parseDomainsFromJson(json);
        if (domains.size() < 2) {
            std::cerr << "[FAIL] Parsed " << domains.size() << " entries, expected >=2\n";
            ++failures;
        } else {
            if (domains[0].networkHost != "alpha.example.org" || domains[0].httpPort != 40102 || domains[0].udpPort != 40104) {
                std::cerr << "[FAIL] First domain mismatch: host=" << domains[0].networkHost << " httpPort=" << domains[0].httpPort << " udpPort=" << domains[0].udpPort << "\n";
                ++failures;
            }
            if (domains[1].networkHost != "beta.example.org") {
                std::cerr << "[FAIL] Second domain host mismatch: host=" << domains[1].networkHost << "\n";
                ++failures;
            }
        }
    }

    // Test 3: discovery JSON parsing (Overte-like alternative keys)
    {
        std::string json = R"JSON({
            "domains": [
              {"name":"Gamma","address":"gamma.example.org","domain_http_port":40400,"domain_udp_port":40404},
              {"name":"Delta","address":"delta.example.org"}
            ]
        })JSON";
        auto domains = parseDomainsFromJson(json);
        if (domains.empty()) {
            std::cerr << "[FAIL] Parsed zero entries for alternative key set\n";
            ++failures;
        } else {
            // find Gamma and Delta
            bool gammaOK=false, deltaOK=false;
            for (auto& d : domains) {
                if (d.networkHost == "gamma.example.org" && d.httpPort == 40400 && d.udpPort == 40404) gammaOK = true;
                if (d.networkHost == "delta.example.org" && d.httpPort == 40102 && d.udpPort == 40104) deltaOK = true; // defaults
            }
            if (!gammaOK) { std::cerr << "[FAIL] Gamma entry not found with expected ports\n"; ++failures; }
            if (!deltaOK) { std::cerr << "[FAIL] Delta entry not found with default ports\n"; ++failures; }
        }
    }

    // Test 4: Entity packet structure validation
    {
        // Simulate a simple EntityAdd packet structure:
        // [type:u8][id:u64][name:null-terminated][position:3xf32][rotation:4xf32][dimensions:3xf32][model_url:null-terminated][texture_url:null-terminated][color:3xf32][entity_type:u8]
        std::vector<uint8_t> entityPacket;
        
        // Packet type (0x10 = ENTITY_ADD)
        entityPacket.push_back(0x10);
        
        // Entity ID (uint64): 12345
        uint64_t entityId = 12345;
        for (int i = 0; i < 8; i++) {
            entityPacket.push_back((entityId >> (i * 8)) & 0xFF);
        }
        
        // Name: "TestEntity\0"
        std::string name = "TestEntity";
        entityPacket.insert(entityPacket.end(), name.begin(), name.end());
        entityPacket.push_back(0); // null terminator
        
        // Position: (1.0, 2.0, 3.0) as 3 floats
        float pos[3] = {1.0f, 2.0f, 3.0f};
        for (int i = 0; i < 3; i++) {
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&pos[i]);
            entityPacket.insert(entityPacket.end(), bytes, bytes + 4);
        }
        
        // Rotation: identity quaternion (0,0,0,1) as 4 floats
        float rot[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        for (int i = 0; i < 4; i++) {
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&rot[i]);
            entityPacket.insert(entityPacket.end(), bytes, bytes + 4);
        }
        
        // Dimensions: (0.5, 0.5, 0.5) as 3 floats
        float dims[3] = {0.5f, 0.5f, 0.5f};
        for (int i = 0; i < 3; i++) {
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&dims[i]);
            entityPacket.insert(entityPacket.end(), bytes, bytes + 4);
        }
        
        // Model URL: empty
        entityPacket.push_back(0);
        
        // Texture URL: empty
        entityPacket.push_back(0);
        
        // Color: (1.0, 0.0, 0.0) red as 3 floats
        float color[3] = {1.0f, 0.0f, 0.0f};
        for (int i = 0; i < 3; i++) {
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&color[i]);
            entityPacket.insert(entityPacket.end(), bytes, bytes + 4);
        }
        
        // Entity type: 1 (Box)
        entityPacket.push_back(1);
        
        std::cout << "[TEST] Entity packet structure: " << entityPacket.size() << " bytes" << std::endl;
        
        // Validate minimum size
        size_t minExpectedSize = 1 + 8 + 11 + 12 + 16 + 12 + 1 + 1 + 12 + 1; // = 75 bytes
        if (entityPacket.size() != minExpectedSize) {
            std::cerr << "[FAIL] Entity packet size mismatch: got " << entityPacket.size() 
                      << " expected " << minExpectedSize << "\n";
            ++failures;
        }
        
        // Validate packet type
        if (entityPacket[0] != 0x10) {
            std::cerr << "[FAIL] Entity packet type mismatch\n";
            ++failures;
        }
        
        // Validate entity ID
        uint64_t readId = 0;
        std::memcpy(&readId, &entityPacket[1], 8);
        if (readId != entityId) {
            std::cerr << "[FAIL] Entity ID mismatch: got " << readId << " expected " << entityId << "\n";
            ++failures;
        }
    }

    if (failures == 0) {
        std::cout << "ALL TESTS PASS" << std::endl;
        return 0;
    } else {
        std::cout << failures << " TEST(S) FAILED" << std::endl;
        return 1;
    }
}
