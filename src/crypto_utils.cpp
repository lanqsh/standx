#include "crypto_utils.h"

#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace standx {

typedef unsigned long long u64;

#define ROL64(a, offset) ((a << offset) ^ (a >> (64 - offset)))

static const u64 keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

static const int keccakf_rotc[24] = {1,  3,  6,  10, 15, 21, 28, 36,
                                     45, 55, 2,  14, 27, 41, 56, 8,
                                     25, 43, 62, 18, 39, 61, 20, 44};

static const int keccakf_piln[24] = {10, 7,  11, 17, 18, 3,  5,  16,
                                     8,  21, 24, 4,  15, 23, 19, 13,
                                     12, 2,  20, 14, 22, 9,  6,  1};

static void keccakf(u64 s[25]) {
  int i, j, round;
  u64 t, bc[5];

  for (round = 0; round < 24; round++) {
    for (i = 0; i < 5; i++)
      bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];

    for (i = 0; i < 5; i++) {
      t = bc[(i + 4) % 5] ^ ROL64(bc[(i + 1) % 5], 1);
      for (j = 0; j < 25; j += 5) s[j + i] ^= t;
    }

    t = s[1];
    for (i = 0; i < 24; i++) {
      j = keccakf_piln[i];
      bc[0] = s[j];
      s[j] = ROL64(t, keccakf_rotc[i]);
      t = bc[0];
    }

    for (j = 0; j < 25; j += 5) {
      for (i = 0; i < 5; i++) bc[i] = s[j + i];
      for (i = 0; i < 5; i++) s[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
    }

    s[0] ^= keccakf_rndc[round];
  }
}

static void keccak_256_impl(const unsigned char* in, size_t inlen,
                            unsigned char* out) {
  u64 s[25];
  unsigned char temp[144];
  size_t rate = 136;
  size_t i;

  memset(s, 0, sizeof(s));

  while (inlen >= rate) {
    for (i = 0; i < rate / 8; i++) {
      s[i] ^= ((u64*)in)[i];
    }
    keccakf(s);
    in += rate;
    inlen -= rate;
  }

  memset(temp, 0, sizeof(temp));
  memcpy(temp, in, inlen);
  temp[inlen] = 0x01;
  temp[rate - 1] |= 0x80;

  for (i = 0; i < rate / 8; i++) {
    s[i] ^= ((u64*)temp)[i];
  }
  keccakf(s);

  memcpy(out, s, 32);
}

static const char* BASE58_ALPHABET =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

void keccak256(const unsigned char* in, size_t inlen, unsigned char* out) {
  keccak_256_impl(in, inlen, out);
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
  keccak_256_impl((const unsigned char*)addr_hex.data(), addr_hex.size(),
                  addr_hash);

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
  keccak_256_impl(pubkey_uncompressed + 1, 64, hash);
  return eip55_checksum_address(hash + 12, 20);
}

}  // namespace standx
