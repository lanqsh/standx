#include "standx_client.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <stdexcept>

#include "auth.h"
#include "http_client.h"
#include "tracer.h"
#include "util.h"

namespace standx {

static std::string mapOrderStatus(const std::string& api_status) {
  if (api_status == "open") return "NEW";
  if (api_status == "canceled") return "CANCELED";
  if (api_status == "filled") return "FILLED";
  if (api_status == "rejected") return "FAILED";
  return "UNKNOWN";
}

StandXClient::StandXClient(const std::string& chain,
                           const std::string& private_key_hex,
                           const std::string& symbol)
    : chain_(chain),
      symbol_(symbol),
      api_base_url_("https://perps.standx.com") {
  http_ = std::make_unique<HttpClient>();
  auth_ = std::make_unique<AuthManager>(chain);
  auth_->set_private_key(private_key_hex);
  access_token_ = auth_->login();

  http_->set_token_refresh_callback([this]() {
    access_token_ = auth_->login();
    return access_token_;
  });
}

StandXClient::~StandXClient() = default;

std::string StandXClient::get_address() const { return auth_->get_address(); }

std::string StandXClient::login() {
  access_token_ = auth_->login();
  return access_token_;
}

std::string StandXClient::request_with_retry(const std::string& url) {
  return http_->get_with_auth(url, access_token_);
}

bool StandXClient::balance(float& availBal, float& totalBal) {
  if (access_token_.empty()) {
    throw std::runtime_error("not logged in, call login() first");
  }

  std::string url = api_base_url_ + "/api/query_balance";

  try {
    std::string response = request_with_retry(url);
    auto json = nlohmann::json::parse(response);

    if (json.contains("cross_available") &&
        json["cross_available"].is_string()) {
      availBal = SafeStof(json["cross_available"].get<std::string>());
    }
    if (json.contains("cross_balance") && json["cross_balance"].is_string()) {
      totalBal = SafeStof(json["cross_balance"].get<std::string>());
    }

    return true;
  } catch (const std::exception& e) {
    ERROR("Failed to query balance: " << e.what());
    return false;
  }
}

bool StandXClient::positions(std::vector<Position>& positions_list) {
  if (access_token_.empty()) {
    throw std::runtime_error("not logged in, call login() first");
  }

  std::string url = api_base_url_ + "/api/query_positions";
  if (!symbol_.empty()) {
    url += "?symbol=" + symbol_;
  }

  try {
    std::string response = request_with_retry(url);
    auto json = nlohmann::json::parse(response);

    positions_list.clear();

    if (json.is_array()) {
      for (const auto& item : json) {
        Position pos;

        float qty = 0.0f;
        if (item.contains("qty") && item["qty"].is_string()) {
          qty = SafeStof(item["qty"].get<std::string>());
        }

        if (qty < 0) {
          pos.positionSide = "SHORT";
          pos.positionAmt = -qty;
        } else {
          pos.positionSide = "LONG";
          pos.positionAmt = qty;
        }

        positions_list.push_back(pos);
      }
    }

    return true;
  } catch (const std::exception& e) {
    ERROR("Error parsing positions response: " << e.what());
    return false;
  }
}

bool StandXClient::detail(Order& order) {
  if (order.cl_ord_id.empty()) {
    ERROR("Order cl_ord_id is empty");
    order.status = "FAILED";
    return true;
  }
  if (access_token_.empty()) {
    throw std::runtime_error("not logged in, call login() first");
  }

  std::string url =
      api_base_url_ + "/api/query_order?cl_ord_id=" + order.cl_ord_id;
  std::string response;
  try {
    response = request_with_retry(url);
    auto json = nlohmann::json::parse(response);

    if (json.contains("status") && json["status"].is_string()) {
      order.status = mapOrderStatus(json["status"].get<std::string>());
    }

    return true;
  } catch (const std::exception& e) {
    ERROR("Error parsing order detail response: " << e.what() << ", response: "
                                                  << response);
    return false;
  }
}

bool StandXClient::unfilledOrders(std::list<Order>& order_list) {
  if (access_token_.empty()) {
    throw std::runtime_error("not logged in, call login() first");
  }

  std::string url = api_base_url_ + "/api/query_open_orders";
  if (!symbol_.empty()) {
    url += "?symbol=" + symbol_;
  }
  std::string response;

  try {
    response = request_with_retry(url);
    auto json = nlohmann::json::parse(response);

    if (json.contains("result") && json["result"].is_array()) {
      order_list.clear();
      for (const auto& item : json["result"]) {
        Order order;

        if (item.contains("side") && item["side"].is_string()) {
          std::string side = item["side"].get<std::string>();
          std::transform(side.begin(), side.end(), side.begin(), ::toupper);
          order.side = side;
        }

        if (item.contains("qty") && item["qty"].is_string()) {
          order.size = SafeStof(item["qty"].get<std::string>());
        }

        if (item.contains("price") && item["price"].is_string()) {
          order.price = SafeStof(item["price"].get<std::string>());
        }

        if (item.contains("reduce_only") && item["reduce_only"].is_boolean()) {
          order.is_reduce_only = item["reduce_only"].get<bool>();
        }

        if (item.contains("cl_ord_id") && item["cl_ord_id"].is_string()) {
          order.cl_ord_id = item["cl_ord_id"].get<std::string>();
        }

        if (item.contains("status") && item["status"].is_string()) {
          order.status = mapOrderStatus(item["status"].get<std::string>());
        }

        if (order.is_reduce_only) {
          if (order.side == "SELL") {
            order.positionSide = "LONG";
          } else if (order.side == "BUY") {
            order.positionSide = "SHORT";
          }
        } else {
          if (order.side == "BUY") {
            order.positionSide = "LONG";
          } else if (order.side == "SELL") {
            order.positionSide = "SHORT";
          }
        }

        order_list.push_back(order);
      }
    }

    return true;
  } catch (const std::exception& e) {
    ERROR("Error parsing unfilled orders response: "
          << e.what() << ", response: " << response);
    return false;
  }
}

bool StandXClient::tickers(Ticker& tk) {
  std::string url = api_base_url_ + "/api/query_symbol_price?symbol=" + symbol_;
  std::string response;

  try {
    response = http_->get(url);
    auto json = nlohmann::json::parse(response);

    if (json.contains("last_price") && json["last_price"].is_string()) {
      tk.last = SafeStof(json["last_price"]);
      return true;
    }

    ERROR("Price field not found in response or is not a string: " << response);
    return false;
  } catch (const std::exception& e) {
    ERROR("Failed to query ticker: " << e.what() << ", response: " << response);
    return false;
  }
}

bool StandXClient::placeOrder(Order& order) {
  if (access_token_.empty()) {
    throw std::runtime_error("not logged in, call login() first");
  }

  nlohmann::json order_json;
  order_json["symbol"] = symbol_;

  std::string side = order.side;
  std::transform(side.begin(), side.end(), side.begin(), ::tolower);
  order_json["side"] = side;

  std::string type = order.type;
  std::transform(type.begin(), type.end(), type.begin(), ::tolower);
  order_json["order_type"] = type;
  order_json["qty"] = std::to_string(order.size);
  order_json["reduce_only"] = order.is_reduce_only;

  if (order.cl_ord_id.empty()) {
    ERROR("placeOrder requires non-empty cl_ord_id");
    return false;
  }
  order_json["cl_ord_id"] = order.cl_ord_id;

  if (type == "market") {
    order_json["time_in_force"] = "ioc";
  } else {
    order_json["time_in_force"] = "alo";
    order_json["price"] = SafeFtos(order.price, PRICE_ACCURACY_INT);
  }

  std::string url = api_base_url_ + "/api/new_order";
  std::string body = order_json.dump();

  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;

  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(8) << (dis(gen) & 0xFFFFFFFF)
     << "-" << std::setw(4) << (dis(gen) & 0xFFFF) << "-" << std::setw(4)
     << ((dis(gen) & 0x0FFF) | 0x4000) << "-" << std::setw(4)
     << ((dis(gen) & 0x3FFF) | 0x8000) << "-" << std::setw(12)
     << (dis(gen) & 0xFFFFFFFFFFFF);
  std::string request_id = ss.str();

  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  std::string timestamp = std::to_string(ms);

  std::string version = "v1";
  std::string message =
      version + "," + request_id + "," + timestamp + "," + body;
  std::string response;
  std::string signature = auth_->sign_ed25519_base64(message);
  std::map<std::string, std::string> extra_headers;
  extra_headers["x-request-sign-version"] = version;
  extra_headers["x-request-id"] = request_id;
  extra_headers["x-request-timestamp"] = timestamp;
  extra_headers["x-request-signature"] = signature;

  try {
    response =
        http_->post_json_with_auth(url, body, access_token_, extra_headers);

    auto json_response = nlohmann::json::parse(response);
    if (json_response.contains("message") &&
        json_response["message"].is_string()) {
      std::string msg = json_response["message"].get<std::string>();
      if (msg == "success") {
        return true;
      }
    }
  } catch (const std::exception& e) {
    ERROR("Failed to place order: " << e.what() << ", response: " << response);
  }

  NOTICE("Failed to place order, response: " << response);
  return false;
}

bool StandXClient::tpOrder(Order& order) {
  if (access_token_.empty()) {
    throw std::runtime_error("not logged in, call login() first");
  }

  nlohmann::json order_json;
  order_json["symbol"] = symbol_;

  std::string side = order.side;
  std::transform(side.begin(), side.end(), side.begin(), ::tolower);
  order_json["side"] = side;

  std::string type = order.type;
  std::transform(type.begin(), type.end(), type.begin(), ::tolower);
  order_json["order_type"] = type;
  order_json["qty"] = std::to_string(order.size);

  if (order.tp_cl_ord_id.empty()) {
    ERROR("tpOrder requires non-empty tp_cl_ord_id");
    return false;
  }
  order_json["cl_ord_id"] = order.tp_cl_ord_id;

  order_json["time_in_force"] = "alo";
  order_json["reduce_only"] = true;
  order_json["price"] = SafeFtos(order.tp_price, PRICE_ACCURACY_INT);

  std::string url = api_base_url_ + "/api/new_order";
  std::string body = order_json.dump();

  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;

  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(8) << (dis(gen) & 0xFFFFFFFF)
     << "-" << std::setw(4) << (dis(gen) & 0xFFFF) << "-" << std::setw(4)
     << ((dis(gen) & 0x0FFF) | 0x4000) << "-" << std::setw(4)
     << ((dis(gen) & 0x3FFF) | 0x8000) << "-" << std::setw(12)
     << (dis(gen) & 0xFFFFFFFFFFFF);
  std::string request_id = ss.str();

  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  std::string timestamp = std::to_string(ms);

  std::string version = "v1";
  std::string message =
      version + "," + request_id + "," + timestamp + "," + body;
  std::string response;

  std::string signature = auth_->sign_ed25519_base64(message);
  std::map<std::string, std::string> extra_headers;
  extra_headers["x-request-sign-version"] = version;
  extra_headers["x-request-id"] = request_id;
  extra_headers["x-request-timestamp"] = timestamp;
  extra_headers["x-request-signature"] = signature;

  try {
    response =
        http_->post_json_with_auth(url, body, access_token_, extra_headers);

    auto json_response = nlohmann::json::parse(response);
    if (json_response.contains("message") &&
        json_response["message"].is_string()) {
      std::string msg = json_response["message"].get<std::string>();
      if (msg == "success") {
        return true;
      }
    }
  } catch (const std::exception& e) {
    ERROR("Failed to place TP order: " << e.what());
  }

  NOTICE("Failed to place TP order, response: " << response);
  return false;
}

void StandXClient::cancelOrder(const std::string& cl_ord_id) {
  if (access_token_.empty()) {
    throw std::runtime_error("not logged in, call login() first");
  }

  if (cl_ord_id.empty()) {
    ERROR("Order cl_ord_id is required for cancel");
    return;
  }

  nlohmann::json cancel_req;
  cancel_req["cl_ord_id"] = cl_ord_id;

  std::string url = api_base_url_ + "/api/cancel_order";
  std::string body = cancel_req.dump();

  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;

  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(8) << (dis(gen) & 0xFFFFFFFF)
     << "-" << std::setw(4) << (dis(gen) & 0xFFFF) << "-" << std::setw(4)
     << ((dis(gen) & 0x0FFF) | 0x4000) << "-" << std::setw(4)
     << ((dis(gen) & 0x3FFF) | 0x8000) << "-" << std::setw(12)
     << (dis(gen) & 0xFFFFFFFFFFFF);
  std::string request_id = ss.str();

  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  std::string timestamp = std::to_string(ms);

  std::string version = "v1";
  std::string message =
      version + "," + request_id + "," + timestamp + "," + body;

  std::string signature = auth_->sign_ed25519_base64(message);

  std::map<std::string, std::string> extra_headers;
  extra_headers["x-request-sign-version"] = version;
  extra_headers["x-request-id"] = request_id;
  extra_headers["x-request-timestamp"] = timestamp;
  extra_headers["x-request-signature"] = signature;

  try {
    http_->post_json_with_auth(url, body, access_token_, extra_headers);
  } catch (const std::exception& e) {
    ERROR("Failed to cancel order " << cl_ord_id << ": " << e.what());
  }
}

}  // namespace standx
