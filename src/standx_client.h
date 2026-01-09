#pragma once

#include <string>
#include <memory>

namespace standx {

class HttpClient;
class AuthManager;

class StandXClient {
public:
    StandXClient(const std::string& chain, const std::string& private_key_hex);
    ~StandXClient();

    // Get user address
    std::string get_address() const;

    // Login and get access token
    std::string login();

    // Query account balance
    std::string query_balance();

    // Query positions
    std::string query_positions(const std::string& symbol = "");

    // Query order by order_id or cl_ord_id (at least one required)
    std::string query_order(int order_id = -1, const std::string& cl_ord_id = "");

    // Get current access token
    std::string get_access_token() const { return access_token_; }

private:
    std::unique_ptr<HttpClient> http_;
    std::unique_ptr<AuthManager> auth_;
    std::string chain_;
    std::string access_token_;
    std::string api_base_url_;
};

} // namespace standx
