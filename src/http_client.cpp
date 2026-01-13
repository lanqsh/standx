#include "http_client.h"

#include <curl/curl.h>

#include <iostream>
#include <stdexcept>

#include "tracer.h"

namespace standx {

HttpClient::HttpClient() {
  curl_ = curl_easy_init();
  if (!curl_) throw std::runtime_error("curl init failed");
  last_response_code_ = 0;
}

HttpClient::~HttpClient() {
  if (curl_) curl_easy_cleanup((CURL*)curl_);
}

void HttpClient::set_token_refresh_callback(TokenRefreshCallback callback) {
  token_refresh_callback_ = callback;
}

size_t HttpClient::write_callback(void* contents, size_t size, size_t nmemb,
                                  void* userp) {
  ((std::string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

std::string HttpClient::perform_request_internal(const std::string& url,
                                                 void* headers_ptr,
                                                 const std::string& method,
                                                 const std::string& post_data,
                                                 bool retry_on_401) {
  CURL* curl = (CURL*)curl_;
  curl_easy_reset(curl);

  struct curl_slist* headers = (struct curl_slist*)headers_ptr;

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

  if (!method.empty()) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
  }

  if (!post_data.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
  }

  INFO_("api", "send " << method << " " << url << ", body:" << post_data);
  CURLcode res = curl_easy_perform(curl);
  INFO_("api", "response: " << response);

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &last_response_code_);

  curl_slist_free_all(headers);

  if (res != CURLE_OK) {
    throw std::runtime_error("curl request failed: " +
                             std::string(curl_easy_strerror(res)));
  }

  if (retry_on_401 && last_response_code_ == 401 && token_refresh_callback_) {
    std::string new_token = token_refresh_callback_();

    struct curl_slist* new_headers = nullptr;
    std::string auth_header = "Authorization: Bearer " + new_token;
    new_headers = curl_slist_append(new_headers, auth_header.c_str());

    if (method == "POST") {
      new_headers =
          curl_slist_append(new_headers, "Content-Type: application/json");
    } else if (method == "DELETE") {
      new_headers = curl_slist_append(new_headers, "Accept: application/json");
    } else {
      new_headers = curl_slist_append(new_headers, "Accept: application/json");
    }

    return perform_request_internal(url, new_headers, method, post_data, false);
  }

  return response;
}

std::string HttpClient::perform_request(const std::string& url,
                                        void* headers_ptr,
                                        const std::string& method,
                                        const std::string& post_data) {
  bool is_auth_request = false;
  struct curl_slist* headers = (struct curl_slist*)headers_ptr;
  for (struct curl_slist* h = headers; h != nullptr; h = h->next) {
    if (std::string(h->data).find("Authorization:") == 0) {
      is_auth_request = true;
      break;
    }
  }

  return perform_request_internal(url, headers_ptr, method, post_data,
                                  is_auth_request);
}

long HttpClient::get_last_response_code() const { return last_response_code_; }

std::string HttpClient::post_json(const std::string& url,
                                  const std::string& json_body) {
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  return perform_request(url, headers, "POST", json_body);
}

std::string HttpClient::post_json_with_auth(const std::string& url,
                                            const std::string& json_body,
                                            const std::string& token) {
  struct curl_slist* headers = nullptr;
  std::string auth_header = "Authorization: Bearer " + token;
  headers = curl_slist_append(headers, auth_header.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");
  return perform_request(url, headers, "POST", json_body);
}

std::string HttpClient::post_json_with_auth(
    const std::string& url, const std::string& json_body,
    const std::string& token,
    const std::map<std::string, std::string>& extra_headers) {
  struct curl_slist* headers = nullptr;
  std::string auth_header = "Authorization: Bearer " + token;
  headers = curl_slist_append(headers, auth_header.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  // Add extra headers
  for (const auto& pair : extra_headers) {
    std::string header = pair.first + ": " + pair.second;
    headers = curl_slist_append(headers, header.c_str());
  }

  return perform_request(url, headers, "POST", json_body);
}

std::string HttpClient::get(const std::string& url) {
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/json");
  return perform_request(url, headers);
}

std::string HttpClient::get_with_auth(const std::string& url,
                                      const std::string& token) {
  struct curl_slist* headers = nullptr;
  std::string auth_header = "Authorization: Bearer " + token;
  headers = curl_slist_append(headers, auth_header.c_str());
  headers = curl_slist_append(headers, "Accept: application/json");
  return perform_request(url, headers);
}

std::string HttpClient::delete_with_auth(const std::string& url,
                                         const std::string& token) {
  struct curl_slist* headers = nullptr;
  std::string auth_header = "Authorization: Bearer " + token;
  headers = curl_slist_append(headers, auth_header.c_str());
  headers = curl_slist_append(headers, "Accept: application/json");
  return perform_request(url, headers, "DELETE");
}

}  // namespace standx
