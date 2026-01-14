#pragma once

#include <string>
#include <map>
#include <functional>

namespace standx {

class HttpClient {
public:
    using TokenRefreshCallback = std::function<std::string()>;

    HttpClient();
    ~HttpClient();

    // POST request with JSON body
    std::string post_json(const std::string& url, const std::string& json_body);

    // POST request with JSON body and authorization header
    std::string post_json_with_auth(const std::string& url, const std::string& json_body, const std::string& token);

    // POST request with JSON body, authorization header, and additional headers
    std::string post_json_with_auth(const std::string& url, const std::string& json_body,
                                     const std::string& token, const std::map<std::string, std::string>& extra_headers);

    // GET request
    std::string get(const std::string& url);

    // GET request with authorization header
    std::string get_with_auth(const std::string& url, const std::string& token);

    // DELETE request with authorization header
    std::string delete_with_auth(const std::string& url, const std::string& token);

    // Get last HTTP response code
    long get_last_response_code() const;

    // Set token refresh callback for automatic retry on 401
    void set_token_refresh_callback(TokenRefreshCallback callback);

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string perform_request(const std::string& url, void* headers, const std::string& method = "", const std::string& post_data = "");
    std::string perform_request_internal(const std::string& url, void* headers, const std::string& method, const std::string& post_data, bool retry_on_401);
    void* curl_;
    long last_response_code_;
    TokenRefreshCallback token_refresh_callback_;
};

} // namespace standx
