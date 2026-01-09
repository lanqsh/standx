#pragma once

#include <string>
#include <map>

namespace standx {

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // POST request with JSON body
    std::string post_json(const std::string& url, const std::string& json_body);

    // GET request
    std::string get(const std::string& url);

    // GET request with authorization header
    std::string get_with_auth(const std::string& url, const std::string& token);

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};

} // namespace standx
