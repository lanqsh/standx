#include "crypto_utils.h"
#include "../deps/tiny_keccak.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace standx {

static const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

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
        for (auto it = b58.rbegin(); (carry != 0 || k < j) && it != b58.rend(); ++it, ++k) {
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

std::string base64url_decode(const std::string& in) {
    std::string s = in;
    for (char& c : s) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (s.size() % 4) s.push_back('=');

    static const std::string B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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

std::string eip55_checksum_address(const unsigned char* addr_bytes, size_t len) {
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

} // namespace standx
