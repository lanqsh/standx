#include "standx_client.h"
#include "http_client.h"
#include "auth.h"
#include <stdexcept>

namespace standx {

StandXClient::StandXClient(const std::string& chain, const std::string& private_key_hex)
    : chain_(chain), api_base_url_("https://perps.standx.com") {
    http_ = std::make_unique<HttpClient>();
    auth_ = std::make_unique<AuthManager>(chain);
    auth_->set_private_key(private_key_hex);
}

StandXClient::~StandXClient() = default;

std::string StandXClient::get_address() const {
    return auth_->get_address();
}

std::string StandXClient::login() {
    access_token_ = auth_->login();
    return access_token_;
}

std::string StandXClient::query_balance() {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    std::string url = api_base_url_ + "/api/query_balance";
    return http_->get_with_auth(url, access_token_);
}

std::string StandXClient::query_positions(const std::string& symbol) {
    if (access_token_.empty()) {
        throw std::runtime_error("not logged in, call login() first");
    }

    std::string url = api_base_url_ + "/api/query_positions";
    if (!symbol.empty()) {
        url += "?symbol=" + symbol;
    }
    return http_->get_with_auth(url, access_token_);
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

    return http_->get_with_auth(url, access_token_);
}

} // namespace standx
