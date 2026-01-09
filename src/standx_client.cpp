#include "standx_client.h"
#include "http_client.h"
#include "auth.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>

namespace standx {

StandXClient::StandXClient(const std::string& chain, const std::string& private_key_hex)
    : chain_(chain), api_base_url_("https://perps.standx.com") {
    http_ = std::make_unique<HttpClient>();
    auth_ = std::make_unique<AuthManager>(chain);
    auth_->set_private_key(private_key_hex);

    // Set token refresh callback for automatic retry on 401
    http_->set_token_refresh_callback([this]() {
        access_token_ = auth_->login();
        return access_token_;
    });
}

StandXClient::~StandXClient() = default;

std::string StandXClient::get_address() const {
    return auth_->get_address();
}

std::string StandXClient::login() {
    access_token_ = auth_->login();
    return access_token_;
}

std::string StandXClient::request_with_retry(const std::string& url) {
    // Token refresh is now handled automatically in HttpClient
    return http_->get_with_auth(url, access_token_);
}

std::string StandXClient::query_balance() {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    std::string url = api_base_url_ + "/api/query_balance";
    return request_with_retry(url);
}

std::string StandXClient::query_positions(const std::string& symbol) {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    std::string url = api_base_url_ + "/api/query_positions";
    if (!symbol.empty()) {
        url += "?symbol=" + symbol;
    }
    return request_with_retry(url);
}

std::string StandXClient::query_order(int order_id, const std::string& cl_ord_id) {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    if (order_id < 0 && cl_ord_id.empty()) {
        throw std::runtime_error("at least one of order_id or cl_ord_id is required");
    }

    std::string url = api_base_url_ + "/api/query_order?";
    if (order_id >= 0) {
        url += "order_id=" + std::to_string(order_id);
    }
    if (!cl_ord_id.empty()) {
        if (order_id >= 0) url += "&";
        url += "cl_ord_id=" + cl_ord_id;
    }

    return request_with_retry(url);
}

std::string StandXClient::query_open_orders(const std::string& symbol) {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    std::string url = api_base_url_ + "/api/query_open_orders";
    if (!symbol.empty()) {
        url += "?symbol=" + symbol;
    }
    return request_with_retry(url);
}

std::string StandXClient::query_symbol_price(const std::string& symbol) {
    if (symbol.empty()) {
        throw std::runtime_error("symbol is required for query_symbol_price");
    }

    std::string url = api_base_url_ + "/api/query_symbol_price?symbol=" + symbol;
    return http_->get(url);
}

std::string StandXClient::new_order(const std::string& symbol, const std::string& side,
                                     const std::string& order_type, const std::string& qty,
                                     const std::string& time_in_force, bool reduce_only,
                                     const std::string& price) {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    if (symbol.empty() || side.empty() || order_type.empty() || qty.empty() || time_in_force.empty()) {
        throw std::runtime_error("symbol, side, order_type, qty, and time_in_force are required");
    }

    nlohmann::json order;
    order["symbol"] = symbol;
    order["side"] = side;
    order["order_type"] = order_type;
    order["qty"] = qty;
    order["time_in_force"] = time_in_force;
    order["reduce_only"] = reduce_only;
    if (!price.empty()) {
        order["price"] = price;
    }

    std::string url = api_base_url_ + "/api/new_order";
    std::string body = order.dump();

    // Body Signature Flow
    // 1. Generate UUID for request_id
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-"
       << std::setw(4) << (dis(gen) & 0xFFFF) << "-"
       << std::setw(4) << ((dis(gen) & 0x0FFF) | 0x4000) << "-"
       << std::setw(4) << ((dis(gen) & 0x3FFF) | 0x8000) << "-"
       << std::setw(12) << (dis(gen) & 0xFFFFFFFFFFFF);
    std::string request_id = ss.str();

    // 2. Get current timestamp (milliseconds)
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::string timestamp = std::to_string(ms);

    // 3. Build message: {version},{id},{timestamp},{payload}
    std::string version = "v1";
    std::string message = version + "," + request_id + "," + timestamp + "," + body;

    // 4. Sign using Ed25519 (matches official TypeScript implementation)
    std::string signature = auth_->sign_ed25519_base64(message);

    std::cout << "\n=== Order Signing Debug ===";
    std::cout << "\nBody: " << body;
    std::cout << "\nRequest ID: " << request_id;
    std::cout << "\nTimestamp: " << timestamp;
    std::cout << "\nMessage to sign: " << message;
    std::cout << "\nSignature (Ed25519 base64): " << signature << " (len=" << signature.length() << ")";
    std::cout << "\n===========================\n";

    // 5. Attach signature to request headers
    std::map<std::string, std::string> extra_headers;
    extra_headers["x-request-sign-version"] = version;
    extra_headers["x-request-id"] = request_id;
    extra_headers["x-request-timestamp"] = timestamp;
    extra_headers["x-request-signature"] = signature;

    // Token refresh is now handled automatically in HttpClient
    return http_->post_json_with_auth(url, body, access_token_, extra_headers);
}

std::string StandXClient::cancel_order(int order_id, const std::string& cl_ord_id) {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    if (order_id < 0 && cl_ord_id.empty()) {
        throw std::runtime_error("at least one of order_id or cl_ord_id is required");
    }

    // Build request body
    nlohmann::json cancel_req;
    if (order_id >= 0) {
        cancel_req["order_id"] = order_id;
    }
    if (!cl_ord_id.empty()) {
        cancel_req["cl_ord_id"] = cl_ord_id;
    }

    std::string url = api_base_url_ + "/api/cancel_order";
    std::string body = cancel_req.dump();

    // Body Signature Flow
    // 1. Generate UUID for request_id
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-"
       << std::setw(4) << (dis(gen) & 0xFFFF) << "-"
       << std::setw(4) << ((dis(gen) & 0x0FFF) | 0x4000) << "-"
       << std::setw(4) << ((dis(gen) & 0x3FFF) | 0x8000) << "-"
       << std::setw(12) << (dis(gen) & 0xFFFFFFFFFFFF);
    std::string request_id = ss.str();

    // 2. Get current timestamp (milliseconds)
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::string timestamp = std::to_string(ms);

    // 3. Build message: {version},{id},{timestamp},{payload}
    std::string version = "v1";
    std::string message = version + "," + request_id + "," + timestamp + "," + body;

    // 4. Sign using Ed25519
    std::string signature = auth_->sign_ed25519_base64(message);

    // 5. Attach signature to request headers
    std::map<std::string, std::string> extra_headers;
    extra_headers["x-request-sign-version"] = version;
    extra_headers["x-request-id"] = request_id;
    extra_headers["x-request-timestamp"] = timestamp;
    extra_headers["x-request-signature"] = signature;

    // Token refresh is now handled automatically in HttpClient
    return http_->post_json_with_auth(url, body, access_token_, extra_headers);
}

} // namespace standx
