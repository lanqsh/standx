#pragma once

#include <string>
#include <vector>

namespace standx {

// Keccak-256 hash
void keccak256(const unsigned char* in, size_t inlen, unsigned char* out);

// Convert hex string to bytes
std::vector<unsigned char> hex_to_bytes(const std::string& hex);

// Convert bytes to hex string
std::string bytes_to_hex(const unsigned char* data, size_t len);

// Base58 encode
std::string base58_encode(const unsigned char* bytes, size_t len);

// Base64url decode
std::string base64url_decode(const std::string& in);

// Generate EIP-55 checksum address from raw address bytes
std::string eip55_checksum_address(const unsigned char* addr_bytes, size_t len);

// Derive Ethereum address from secp256k1 public key (uncompressed, 65 bytes)
std::string derive_eth_address(const unsigned char* pubkey_uncompressed);

} // namespace standx
