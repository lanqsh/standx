#include "auth.h"
#include "crypto_utils.h"
#include "http_client.h"
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <sodium.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/sha.h>
#include <stdexcept>
#include <cstring>

using json = nlohmann::json;

namespace standx {

struct AuthManager::Impl {
    secp256k1_context* ctx;
    unsigned char ed25519_pk[crypto_sign_PUBLICKEYBYTES];  // Ed25519 public key
    unsigned char ed25519_sk[crypto_sign_SECRETKEYBYTES];  // Ed25519 secret key

    Impl() {
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
        if (!ctx) throw std::runtime_error("failed to create secp256k1 context");
    }

    ~Impl() {
        if (ctx) secp256k1_context_destroy(ctx);
    }
};

AuthManager::AuthManager(const std::string& chain)
    : impl_(new Impl()), chain_(chain), auth_base_url_("https://api.standx.com") {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }

    crypto_sign_keypair(impl_->ed25519_pk, impl_->ed25519_sk);
}

AuthManager::~AuthManager() {
    delete impl_;
}

void AuthManager::set_private_key(const std::string& private_key_hex) {
    private_key_bytes_ = hex_to_bytes(private_key_hex);
    if (private_key_bytes_.size() != 32) {
        throw std::runtime_error("private key must be 32 bytes");
    }

    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(impl_->ctx, &pubkey, private_key_bytes_.data())) {
        throw std::runtime_error("failed to create public key from private key");
    }

    unsigned char pubser[65];
    size_t pubserlen = 65;
    secp256k1_ec_pubkey_serialize(impl_->ctx, pubser, &pubserlen, &pubkey, SECP256K1_EC_UNCOMPRESSED);

    address_ = derive_eth_address(pubser);
}

std::string AuthManager::get_address() const {
    return address_;
}

std::string AuthManager::login(int expires_seconds) {
    if (private_key_bytes_.empty()) {
        throw std::runtime_error("private key not set");
    }

    std::string request_id = base58_encode(impl_->ed25519_pk, crypto_sign_PUBLICKEYBYTES);

    HttpClient http;
    std::string prepare_url = auth_base_url_ + "/v1/offchain/prepare-signin?chain=" + chain_;
    json jreq;
    jreq["address"] = address_;
    jreq["requestId"] = request_id;

    std::string resp = http.post_json(prepare_url, jreq.dump());
    json jresp = json::parse(resp);

    if (!jresp.value("success", false)) {
        throw std::runtime_error("prepare-signin failed: " + resp);
    }

    std::string signed_data = jresp.value("signedData", "");
    if (signed_data.empty()) {
        throw std::runtime_error("empty signedData");
    }

    if (!verify_jwt(signed_data)) {
        throw std::runtime_error("JWT verification failed");
    }

    size_t p1 = signed_data.find('.');
    size_t p2 = signed_data.find('.', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) {
        throw std::runtime_error("invalid JWT format");
    }

    std::string payload_b64 = signed_data.substr(p1 + 1, p2 - p1 - 1);
    std::string payload_json = base64url_decode(payload_b64);
    json payload = json::parse(payload_json);
    std::string message = payload.value("message", "");

    if (message.empty()) {
        throw std::runtime_error("payload.message empty");
    }

    std::string signature = sign_message(message);

    std::string login_url = auth_base_url_ + "/v1/offchain/login?chain=" + chain_;
    json jlogin;
    jlogin["signature"] = signature;
    jlogin["signedData"] = signed_data;
    jlogin["expiresSeconds"] = expires_seconds;

    std::string login_resp = http.post_json(login_url, jlogin.dump());
    json jlogin_resp = json::parse(login_resp);

    std::string access_token = jlogin_resp.value("accessToken", jlogin_resp.value("token", ""));
    if (access_token.empty()) {
        throw std::runtime_error("login failed: " + login_resp);
    }

    return access_token;
}

std::string AuthManager::sign_message(const std::string& message) {
    if (private_key_bytes_.empty()) {
        throw std::runtime_error("private key not set");
    }

    // EIP-191 personal_sign
    std::string prefix;
    prefix.push_back((char)0x19);
    prefix += "Ethereum Signed Message:\n" + std::to_string(message.size());
    std::string prefixed = prefix + message;

    unsigned char msghash[32];
    keccak256((const unsigned char*)prefixed.data(), prefixed.size(), msghash);

    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(impl_->ctx, &sig, msghash, private_key_bytes_.data(), NULL, NULL)) {
        throw std::runtime_error("secp256k1 sign failed");
    }

    unsigned char sig64[64];
    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(impl_->ctx, sig64, &recid, &sig);

    unsigned char outsig[65];
    memcpy(outsig, sig64, 64);
    outsig[64] = (unsigned char)(recid + 27);
    return "0x" + bytes_to_hex(outsig, 65);
}

std::string AuthManager::sign_message_base64(const std::string& message) {
    if (private_key_bytes_.empty()) {
        throw std::runtime_error("private key not set");
    }

    std::string prefix;
    prefix.push_back((char)0x19);
    prefix += "Ethereum Signed Message:\n" + std::to_string(message.size());
    std::string prefixed = prefix + message;

    unsigned char msghash[32];
    keccak256((const unsigned char*)prefixed.data(), prefixed.size(), msghash);

    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(impl_->ctx, &sig, msghash, private_key_bytes_.data(), NULL, NULL)) {
        throw std::runtime_error("secp256k1 sign failed");
    }

    unsigned char sig64[64];
    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(impl_->ctx, sig64, &recid, &sig);

    unsigned char outsig[65];
    memcpy(outsig, sig64, 64);
    outsig[64] = (unsigned char)(recid + 27);

    return base64_encode(outsig, 65);
}

std::string AuthManager::sign_hash_base64(const std::string& message) {
    if (private_key_bytes_.empty()) {
        throw std::runtime_error("private key not set");
    }

    unsigned char msghash[32];
    keccak256((const unsigned char*)message.data(), message.size(), msghash);

    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(impl_->ctx, &sig, msghash, private_key_bytes_.data(), NULL, NULL)) {
        throw std::runtime_error("secp256k1 sign failed");
    }

    unsigned char sig64[64];
    int recid = 0;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(impl_->ctx, sig64, &recid, &sig);

    unsigned char outsig[65];
    memcpy(outsig, sig64, 64);
    outsig[64] = (unsigned char)(recid + 27);  // Ethereum convention

    // Base64 encode
    return base64_encode(outsig, 65);
}

std::string AuthManager::sign_ecdsa_64_base64(const std::string& message) {
    if (private_key_bytes_.empty()) {
        throw std::runtime_error("private key not set");
    }

    unsigned char msghash[32];
    keccak256((const unsigned char*)message.data(), message.size(), msghash);

    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_sign(impl_->ctx, &sig, msghash, private_key_bytes_.data(), NULL, NULL)) {
        throw std::runtime_error("secp256k1 sign failed");
    }

    // Serialize to compact format (64 bytes: r + s)
    unsigned char sig64[64];
    secp256k1_ecdsa_signature_serialize_compact(impl_->ctx, sig64, &sig);

    // Base64 encode (only 64 bytes, no recovery id)
    return base64_encode(sig64, 64);
}

std::string AuthManager::sign_ed25519_base64(const std::string& message) {
    if (private_key_bytes_.empty()) {
        throw std::runtime_error("private key not set");
    }

    // Ed25519 signature (64 bytes)
    unsigned char signature[crypto_sign_BYTES];
    unsigned long long sig_len;

    crypto_sign_detached(
        signature,
        &sig_len,
        (const unsigned char*)message.data(),
        message.size(),
        impl_->ed25519_sk
    );

    // Base64 encode (64 bytes)
    return base64_encode(signature, crypto_sign_BYTES);
}

bool AuthManager::verify_jwt(const std::string& signed_data) {
    // Simplified JWT verification (can be expanded)
    return !signed_data.empty();
}

} // namespace standx
