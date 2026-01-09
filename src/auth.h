#pragma once

#include <string>
#include <vector>

namespace standx {

class AuthManager {
public:
    AuthManager(const std::string& chain);
    ~AuthManager();

    // Set private key (hex format with or without 0x prefix)
    void set_private_key(const std::string& private_key_hex);

    // Get Ethereum address (EIP-55 checksum format)
    std::string get_address() const;

    // Sign SIWE message and login, returns access token
    std::string login(int expires_seconds = 604800);

    // Sign arbitrary message using EIP-191 personal_sign
    std::string sign_message(const std::string& message);

    // Sign message and return base64 encoded signature (65 bytes: r+s+v)
    std::string sign_message_base64(const std::string& message);

    // Sign message hash directly (no EIP-191 prefix) and return base64
    std::string sign_hash_base64(const std::string& message);

    // Sign using standard ECDSA (64 bytes r+s, no recovery id) and return base64
    std::string sign_ecdsa_64_base64(const std::string& message);

    // Sign using Ed25519 (libsodium) and return base64 - matches official API
    std::string sign_ed25519_base64(const std::string& message);

    // Verify JWT signedData
    bool verify_jwt(const std::string& signed_data);

private:
    struct Impl;
    Impl* impl_;

    std::string chain_;
    std::string address_;
    std::vector<unsigned char> private_key_bytes_;
    std::string auth_base_url_;
};

} // namespace standx
