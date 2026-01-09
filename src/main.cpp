#include <sodium.h>
#include <curl/curl.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cassert>
#include <cstdlib>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::string base58_encode(const unsigned char* bytes, size_t len) {
    std::vector<unsigned char> input(bytes, bytes + len);
    // Count leading zeros
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

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string http_post_json(const std::string& url, const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl init failed");
    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::string e = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw std::runtime_error("curl failed: " + e);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

std::string base64url_decode(const std::string& in) {
    std::string s = in;
    // replace -_ with +/
    for (char& c : s) { if (c == '-') c = '+'; else if (c == '_') c = '/'; }
    while (s.size() % 4) s.push_back('=');
    std::string out;
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* bio = BIO_new_mem_buf(s.data(), (int)s.size());
    bio = BIO_push(b64, bio);
    out.resize(s.size());
    int r = BIO_read(bio, &out[0], (int)s.size());
    if (r > 0) out.resize(r); else out.clear();
    BIO_free_all(bio);
    return out;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \n\r\t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \n\r\t");
    return s.substr(a, b - a + 1);
}

int main(int argc, char** argv) {
    if (sodium_init() < 0) {
        std::cerr << "libsodium init failed\n";
        return 1;
    }

    if (argc < 4) {
        std::cerr << "Usage: standx_client <chain> <wallet_private_key_hex> <positions_path_optional>\n";
        return 1;
    }
    std::string chain = argv[1];
    std::string wallet_priv_hex = argv[2];
    std::string positions_path = (argc >= 4 ? argv[3] : "");

    // 1) generate ed25519 keypair
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(pk, sk);
    std::string requestId = base58_encode(pk, crypto_sign_PUBLICKEYBYTES);

    // 2) call prepare-signin
    std::string prepare_url = "https://api.standx.com/v1/offchain/prepare-signin?chain=" + chain;
    json jreq;
    jreq["address"] = ""; // TODO: user fill wallet address if needed (we only use wallet private key below)
    jreq["requestId"] = requestId;
    std::string resp = http_post_json(prepare_url, jreq.dump());
    json jresp = json::parse(resp);
    if (!jresp.value("success", false)) {
        std::cerr << "prepare-signin failed: " << resp << "\n";
        return 1;
    }
    std::string signedData = jresp.value("signedData", "");
    if (signedData.empty()) {
        std::cerr << "empty signedData\n";
        return 1;
    }

    // 3) parse JWT payload (header.payload.signature)
    size_t first = signedData.find('.');
    size_t second = signedData.find('.', first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        std::cerr << "invalid JWT\n";
        return 1;
    }
    std::string payload_b64 = signedData.substr(first + 1, second - first - 1);

    // base64url decode - using simple platform independent fallback
    // We'll use a small decode routine via OpenSSL BIO to avoid extra deps.
    // Build a small base64url->base64 converter
    std::string pb64 = payload_b64;
    for (char& c : pb64) if (c == '-') c = '+'; else if (c == '_') c = '/';
    while (pb64.size() % 4) pb64.push_back('=');
    // Use curl's base64? We'll use a quick decoder via std::string
    // For portability, we use a tiny base64 decoder implemented here.

    // Simple base64 decode (not optimized)
    static const std::string B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto b64decode = [&](const std::string& in)->std::string{
        std::string out;
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[(unsigned char)B64[i]] = i;
        int val=0, valb=-8;
        for (unsigned char c : in) {
            if (T[c] == -1) break;
            val = (val<<6) + T[c];
            valb += 6;
            if (valb>=0) {
                out.push_back(char((val>>valb)&0xFF));
                valb-=8;
            }
        }
        return out;
    };

    std::string payload_json = b64decode(pb64);
    json payload = json::parse(payload_json);
    std::string message = payload.value("message", "");
    if (message.empty()) {
        std::cerr << "payload.message empty\n";
        return 1;
    }

    // 4) Sign message using ethers through node: create temp JS file
    // JS will use ethers to sign: node sign_message.js <privkey> <message>
    std::string tmp_js = "./_sign_temp.js";
    std::ofstream ofs(tmp_js);
    ofs << "import { ethers } from \"ethers\";\n";
    ofs << "(async ()=>{\n";
    ofs << "  const pk = process.argv[2];\n";
    ofs << "  const msg = process.argv[3];\n";
    ofs << "  const wallet = new ethers.Wallet(pk);\n";
    ofs << "  const sig = await wallet.signMessage(msg);\n";
    ofs << "  console.log(sig);\n";
    ofs << "})();\n";
    ofs.close();

    // run node to sign
    std::string cmd = "node " + tmp_js + " '" + wallet_priv_hex + "' '" + message + "' 2>/dev/null";
    // On Windows, redirection differs; avoid stderr redirect to keep compatibility
    #ifdef _WIN32
    cmd = "node " + tmp_js + " " + wallet_priv_hex + " \"" + message + "\"";
    #endif

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "failed to run node for signing. Ensure Node + ethers installed.\n";
        return 1;
    }
    char buf[4096];
    std::string signature;
    while (fgets(buf, sizeof(buf), pipe) != NULL) {
        signature += buf;
    }
    pclose(pipe);
    signature = trim(signature);
    if (signature.empty()) {
        std::cerr << "signature empty; ensure Node + ethers installed and the private key is correct.\n";
        return 1;
    }

    // 5) submit login
    std::string login_url = "https://api.standx.com/v1/offchain/login?chain=" + chain;
    json jlogin;
    jlogin["signature"] = signature;
    jlogin["signedData"] = signedData;
    jlogin["expiresSeconds"] = 604800;
    std::string login_resp = http_post_json(login_url, jlogin.dump());
    json jlogin_resp = json::parse(login_resp);
    std::string access_token = jlogin_resp.value("accessToken", jlogin_resp.value("token", ""));
    if (access_token.empty()) {
        std::cerr << "login failed: " << login_resp << "\n";
        return 1;
    }
    std::cout << "Access token: " << access_token << "\n";

    // 6) query positions if provided
    if (!positions_path.empty()) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "curl init failed\n"; return 1;
        }
        std::string resp2;
        struct curl_slist* headers = nullptr;
        std::string auth = "Authorization: Bearer " + access_token;
        headers = curl_slist_append(headers, auth.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, positions_path.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp2);
        CURLcode r = curl_easy_perform(curl);
        if (r != CURLE_OK) std::cerr << "positions request failed: " << curl_easy_strerror(r) << "\n";
        else std::cout << "Positions response:\n" << resp2 << "\n";
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    // cleanup temp js
    std::remove(tmp_js.c_str());
    return 0;
}
