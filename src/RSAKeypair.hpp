// RSAKeypair.hpp - RSA keypair generation and signing for Overte authentication
#pragma once

#include <vector>
#include <string>
#include <cstdint>

class RSAKeypair {
public:
    RSAKeypair();
    ~RSAKeypair();
    
    // Generate a new 2048-bit RSA keypair
    bool generate();
    
    // Sign plaintext with SHA256 + RSA
    std::vector<uint8_t> sign(const std::vector<uint8_t>& plaintext) const;
    
    // Get DER-encoded keys
    std::vector<uint8_t> getPublicKeyDER() const { return m_publicKey; }
    std::vector<uint8_t> getPrivateKeyDER() const { return m_privateKey; }
    
    // Set keys from DER encoding (for loading from file)
    void setKeys(const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& privateKey);
    
    // Check if keypair is valid
    bool isValid() const { return !m_privateKey.empty() && !m_publicKey.empty(); }
    
private:
    std::vector<uint8_t> m_publicKey;
    std::vector<uint8_t> m_privateKey;
};
