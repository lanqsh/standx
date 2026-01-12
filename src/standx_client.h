#pragma once

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "data.h"

namespace standx {

class HttpClient;
class AuthManager;

class StandXClient {
 public:
  StandXClient(const std::string& chain, const std::string& private_key_hex,
               const std::string& symbol);
  ~StandXClient();

  std::string get_address() const;

  std::string getInstId() const { return symbol_; }

  std::string login();

  bool positions(std::vector<Position>& positions_list);

  bool detail(Order& order);

  bool unfilledOrders(std::list<Order>& order_list);

  bool tickers(Ticker& tk);

  bool placeOrder(Order& order);

  bool tpOrder(Order& order);

  void cancelOrder(const std::string& id);

  std::string get_access_token() const { return access_token_; }

  AuthManager* get_auth_manager() const { return auth_.get(); }

  bool balance(float& availBal, float& totalBal);

 private:
  std::string request_with_retry(const std::string& url);

  std::unique_ptr<HttpClient> http_;
  std::unique_ptr<AuthManager> auth_;
  std::string chain_;
  std::string symbol_;
  std::string access_token_;
  std::string api_base_url_;
};

}  // namespace standx
