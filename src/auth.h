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
