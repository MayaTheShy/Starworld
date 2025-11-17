// RSAKeypair.cpp - RSA keypair generation and signing
#include "RSAKeypair.hpp"

#include <iostream>
#include <cstring>
#include <ctime>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/objects.h>  // For NID_sha256

RSAKeypair::RSAKeypair() {}

RSAKeypair::~RSAKeypair() {}

bool RSAKeypair::generate() {
    // Create RSA structure
    RSA* keyPair = RSA_new();
    if (!keyPair) {
        std::cerr << "[RSAKeypair] Failed to create RSA structure" << std::endl;
        return false;
    }
    
    // Create exponent (65537 is standard)
    BIGNUM* exponent = BN_new();
    if (!exponent) {
        RSA_free(keyPair);
        std::cerr << "[RSAKeypair] Failed to create BIGNUM for exponent" << std::endl;
        return false;
    }
    
    const unsigned long RSA_KEY_EXPONENT = 65537;
    BN_set_word(exponent, RSA_KEY_EXPONENT);
    
    // Seed random number generator
    srand(time(NULL));
    
    // Generate 2048-bit keypair
    const int RSA_KEY_BITS = 2048;
    std::cout << "[RSAKeypair] Generating " << RSA_KEY_BITS << "-bit RSA keypair..." << std::endl;
    
    int result = RSA_generate_key_ex(keyPair, RSA_KEY_BITS, exponent, NULL);
    BN_free(exponent);
    
    if (result != 1) {
        std::cerr << "[RSAKeypair] Failed to generate keypair: " << ERR_get_error() << std::endl;
        RSA_free(keyPair);
        return false;
    }
    
    std::cout << "[RSAKeypair] Keypair generated successfully" << std::endl;
    
    // Extract public key in DER format
    unsigned char* publicKeyDER = nullptr;
    int publicKeyLength = i2d_RSAPublicKey(keyPair, &publicKeyDER);
    
    if (publicKeyLength <= 0) {
        std::cerr << "[RSAKeypair] Failed to encode public key: " << ERR_get_error() << std::endl;
        RSA_free(keyPair);
        return false;
    }
    
    m_publicKey.assign(publicKeyDER, publicKeyDER + publicKeyLength);
    OPENSSL_free(publicKeyDER);
    
    // Extract private key in DER format
    unsigned char* privateKeyDER = nullptr;
    int privateKeyLength = i2d_RSAPrivateKey(keyPair, &privateKeyDER);
    
    if (privateKeyLength <= 0) {
        std::cerr << "[RSAKeypair] Failed to encode private key: " << ERR_get_error() << std::endl;
        RSA_free(keyPair);
        return false;
    }
    
    m_privateKey.assign(privateKeyDER, privateKeyDER + privateKeyLength);
    OPENSSL_free(privateKeyDER);
    
    RSA_free(keyPair);
    
    std::cout << "[RSAKeypair] Public key: " << publicKeyLength << " bytes" << std::endl;
    std::cout << "[RSAKeypair] Private key: " << privateKeyLength << " bytes" << std::endl;
    
    return true;
}

std::vector<uint8_t> RSAKeypair::sign(const std::vector<uint8_t>& plaintext) const {
    if (m_privateKey.empty()) {
        std::cerr << "[RSAKeypair] Cannot sign: no private key" << std::endl;
        return {};
    }
    
    // Load private key from DER
    const unsigned char* privateKeyData = m_privateKey.data();
    RSA* rsaPrivateKey = d2i_RSAPrivateKey(nullptr, &privateKeyData, m_privateKey.size());
    
    if (!rsaPrivateKey) {
        std::cerr << "[RSAKeypair] Failed to load private key for signing" << std::endl;
        return {};
    }
    
    // Hash the plaintext with SHA256
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(plaintext.data(), plaintext.size(), hash);
    
    // Allocate signature buffer
    std::vector<uint8_t> signature(RSA_size(rsaPrivateKey));
    unsigned int signatureLength = 0;
    
    // Sign the hash
    int result = RSA_sign(NID_sha256,
                         hash,
                         SHA256_DIGEST_LENGTH,
                         signature.data(),
                         &signatureLength,
                         rsaPrivateKey);
    
    RSA_free(rsaPrivateKey);
    
    if (result != 1) {
        std::cerr << "[RSAKeypair] Signing failed: " << ERR_get_error() << std::endl;
        return {};
    }
    
    signature.resize(signatureLength);
    return signature;
}

void RSAKeypair::setKeys(const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& privateKey) {
    m_publicKey = publicKey;
    m_privateKey = privateKey;
}
