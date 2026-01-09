#include "crypto_utils.h"

#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "tiny_keccak.h"
#include "Poco/LocalDateTime.h"
#include "Poco/Thread.h"
#include "data.h"
#include "tracer.h"

namespace standx {

namespace {
std::map<char, std::string> kEscapingMap = {
    {' ', "%20"}, {'"', "%22"}, {'#', "%23"},  {'%', "%25"}, {'&', "%26"},
    {'(', "%28"}, {')', "%29"}, {'+', "%2B"},  {',', "%2C"}, {'/', "%2F"},
    {':', "%3A"}, {';', "%3B"}, {'<', "%3C"},  {'=', "%3D"}, {'>', "%3E"},
    {'?', "%3F"}, {'@', "%40"}, {'\\', "%5C"}, {'|', "%7C"}, {'`', "\\`"},
    {'*', "\\*"}, {'$', "\\$"}, {'[', "%5B"},  {']', "%5D"}, {'^', "%5E"},
    {'{', "%7B"}, {'}', "%7D"}, {'~', "%7E"}};
}  // namespace

static const char* BASE58_ALPHABET =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

void keccak256(const unsigned char* in, size_t inlen, unsigned char* out) {
  keccak_256(in, inlen, out);
}

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
  auto hex_char_to_int = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
  };

  std::string s = hex;
  if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s = s.substr(2);
  }
  if (s.size() % 2) throw std::runtime_error("invalid hex length");

  std::vector<unsigned char> out;
  out.reserve(s.size() / 2);
  for (size_t i = 0; i < s.size(); i += 2) {
    int hi = hex_char_to_int(s[i]);
    int lo = hex_char_to_int(s[i + 1]);
    if (hi < 0 || lo < 0) throw std::runtime_error("invalid hex character");
    out.push_back((unsigned char)((hi << 4) | lo));
  }
  return out;
}

std::string bytes_to_hex(const unsigned char* data, size_t len) {
  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  for (size_t i = 0; i < len; ++i) {
    ss << std::setw(2) << (int)data[i];
  }
  return ss.str();
}

std::string base58_encode(const unsigned char* bytes, size_t len) {
  std::vector<unsigned char> input(bytes, bytes + len);
  size_t zeros = 0;
  while (zeros < input.size() && input[zeros] == 0) ++zeros;

  std::vector<unsigned char> b58((input.size() - zeros) * 138 / 100 + 1);
  size_t j = 0;
  for (size_t i = zeros; i < input.size(); ++i) {
    int carry = input[i];
    size_t k = 0;
    for (auto it = b58.rbegin(); (carry != 0 || k < j) && it != b58.rend();
         ++it, ++k) {
      carry += 256 * (*it);
      *it = carry % 58;
      carry /= 58;
    }
    j = k;
  }

  std::string result;
  result.reserve(zeros + j);
  for (size_t i = 0; i < zeros; ++i) result.push_back('1');
  auto it = b58.begin();
  while (it != b58.end() && *it == 0) ++it;
  for (; it != b58.end(); ++it) result.push_back(BASE58_ALPHABET[*it]);
  return result;
}

std::string base64_encode(const unsigned char* data, size_t len) {
  static const char* b64chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((len + 2) / 3) * 4);

  for (size_t i = 0; i < len; i += 3) {
    unsigned int val = (data[i] << 16);
    if (i + 1 < len) val |= (data[i + 1] << 8);
    if (i + 2 < len) val |= data[i + 2];

    result.push_back(b64chars[(val >> 18) & 0x3F]);
    result.push_back(b64chars[(val >> 12) & 0x3F]);
    result.push_back((i + 1 < len) ? b64chars[(val >> 6) & 0x3F] : '=');
    result.push_back((i + 2 < len) ? b64chars[val & 0x3F] : '=');
  }

  return result;
}

std::string base64url_decode(const std::string& in) {
  std::string s = in;
  for (char& c : s) {
    if (c == '-')
      c = '+';
    else if (c == '_')
      c = '/';
  }
  while (s.size() % 4) s.push_back('=');

  static const std::string B64 =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; ++i) T[(unsigned char)B64[i]] = i;

  std::string out;
  int val = 0, valb = -8;
  for (unsigned char c : s) {
    if (T[c] == -1) break;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

std::string eip55_checksum_address(const unsigned char* addr_bytes,
                                   size_t len) {
  if (len != 20) throw std::runtime_error("address must be 20 bytes");

  std::string addr_hex = bytes_to_hex(addr_bytes, len);
  unsigned char addr_hash[32];
  keccak_256((const unsigned char*)addr_hex.data(), addr_hex.size(), addr_hash);

  std::string result = "0x";
  for (size_t i = 0; i < 40; i++) {
    char c = addr_hex[i];
    if (c >= 'a' && c <= 'f') {
      int hash_nibble = (addr_hash[i / 2] >> (i % 2 ? 0 : 4)) & 0xf;
      if (hash_nibble >= 8) {
        c = c - 'a' + 'A';
      }
    }
    result += c;
  }
  return result;
}

std::string derive_eth_address(const unsigned char* pubkey_uncompressed) {
  unsigned char hash[32];
  keccak_256(pubkey_uncompressed + 1, 64, hash);
  return eip55_checksum_address(hash + 12, 20);
}

// Utility functions from util.cpp
std::string hex_encode(const unsigned char* data, size_t length) {
  std::ostringstream hexStream;
  for (size_t i = 0; i < length; ++i) {
    hexStream << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
  }
  return hexStream.str();
}

std::string sha512(const std::string& input) {
  unsigned char hash[SHA512_DIGEST_LENGTH];
  SHA512(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(),
         hash);
  return hex_encode(hash, SHA512_DIGEST_LENGTH);
}

std::string generate_signature(const std::string& key,
                               const std::string& data) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  HMAC(EVP_sha256(), key.c_str(), key.length(), (unsigned char*)data.c_str(),
       data.length(), digest, NULL);

  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  }
  return ss.str();
}

std::string get_timestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_seconds = system_clock::to_time_t(now);
  std::tm tm = {};

  localtime_r(&now_seconds, &tm);
  auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

uint64_t safe_stoll(const std::string& str) {
  if (str.empty()) return 0;

  try {
    return std::stoull(str);
  } catch (...) {
    ERROR("safe_stoll error: " << str);
    return 0;
  }
}

int safe_stoi(const std::string& str) {
  if (str.empty()) return 0;

  try {
    return std::stoi(str);
  } catch (...) {
    ERROR("safe_stoi error: " << str);
    return 0;
  }
}

float safe_stof(const std::string& str) {
  if (str.empty()) return 0.0f;

  try {
    return std::stof(str);
  } catch (...) {
    ERROR("safe_stof error: " << str);
    return 0.0f;
  }
}

std::string safe_ftos(float value, int places) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(places);
  oss << value;
  return oss.str();
}

bool are_floats_equal(float a, float b, float epsilon) {
  return std::fabs(a - b) < epsilon;
}

std::string adjust_decimal_places(float num, const std::string& epsilon) {
  float epsilon_float = safe_stof(epsilon);
  int precision = epsilon.size() - 2;

  num *= std::pow(10, precision);
  epsilon_float *= std::pow(10, precision);

  if (num / epsilon_float != 0) {
    num = std::round(num / epsilon_float) * epsilon_float;
  }

  num *= std::pow(10, -precision);

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(precision) << num;

  return oss.str();
}

std::string convert_remark(const std::string& remark) {
  std::string res;
  for (int i = 0; i < remark.size(); ++i) {
    char c = remark.at(i);
    auto it = kEscapingMap.find(c);
    if (it != kEscapingMap.end()) {
      res += kEscapingMap[c];
    } else {
      res += remark.at(i);
    }
  }
  return res;
}

void send_message(const std::string& message, bool force) {
  std::string endpoint = "curl -s -o /dev/null " + kConfig.bark_server;
  std::string ring = "?level=critical&volume=1";

  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm* local_time = std::localtime(&now_time);
  int current_hour = local_time->tm_hour;
  if (!force && current_hour < 8) {
    ring = "";
  }

  std::string cmd = convert_remark(message);
  cmd = endpoint + cmd + ring;
  system(cmd.c_str());
  NOTICE("Send message: " << cmd);
}

}  // namespace standx
