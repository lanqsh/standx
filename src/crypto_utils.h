#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Poco/LocalDateTime.h"
#include "Poco/Thread.h"
#include "data.h"

namespace standx {

// Time and thread macros
#define THIS_YEAR Poco::LocalDateTime().year()
#define THIS_MONTH Poco::LocalDateTime().month()
#define THIS_DAY Poco::LocalDateTime().day()
#define THIS_HOUR Poco::LocalDateTime().hour()
#define THIS_MINUTE Poco::LocalDateTime().minute()
#define THIS_SECOND Poco::LocalDateTime().second()
#define THIS_MILLISEC Poco::LocalDateTime().millisecond()
#define SLEEP_MS(ms) Poco::Thread::sleep(ms)

// Enum operators
template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
  const T& enumValue, const typename std::common_type<int, uint32_t>::type intValue) {
  return static_cast<typename std::underlying_type<T>::type>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
  const typename std::common_type<int, uint32_t>::type intValue, const T& enumValue) {
  return static_cast<typename std::underlying_type<T>::type>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, std::ostream&>::type operator<<(
  std::ostream& os, T value) {
  return os << static_cast<typename std::underlying_type<T>::type>(value);
}

// Cryptographic functions

// Keccak-256 hash
void keccak256(const unsigned char* in, size_t inlen, unsigned char* out);

// Convert hex string to bytes
std::vector<unsigned char> hex_to_bytes(const std::string& hex);

// Convert bytes to hex string
std::string bytes_to_hex(const unsigned char* data, size_t len);

// Base58 encode
std::string base58_encode(const unsigned char* bytes, size_t len);

// Base64 encode
std::string base64_encode(const unsigned char* data, size_t len);

// Base64url decode
std::string base64url_decode(const std::string& in);

// Generate EIP-55 checksum address from raw address bytes
std::string eip55_checksum_address(const unsigned char* addr_bytes, size_t len);

// Derive Ethereum address from secp256k1 public key (uncompressed, 65 bytes)
std::string derive_eth_address(const unsigned char* pubkey_uncompressed);

std::string generate_signature(const std::string& key, const std::string& data);
std::string get_timestamp();
uint64_t safe_stoll(const std::string& str);
int safe_stoi(const std::string& str);
float safe_stof(const std::string& str);
std::string safe_ftos(float value, int places);
bool are_floats_equal(float a, float b, float epsilon = 1e-6);
std::string adjust_decimal_places(float num, const std::string& epsilon);
std::string convert_remark(const std::string& remark);
void send_message(const std::string& message, bool force = false);

}  // namespace standx
