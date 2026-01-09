#pragma once

#include <cstdint>
#include <string>

namespace standx {

// Constants
constexpr double DEFAULT_CONTRACT_SIZE = 0.01;
constexpr int ORDER_NUM = 10;
constexpr double MAX_ORDER_NUM_FACTOR = 1.5;
constexpr int PRICE_ACCURACY_INT = 2;
constexpr double PRICE_ACCURACY_FLOAT = 0.01;

struct Config {
  double lever;
  double min_avail_bal;
  std::string black_list;
  std::string white_list;

  std::string api_key;
  std::string secret_key;

  std::string log_name;
  std::string log_size;
  std::string log_level;

  std::string bark_server;

  double sub_btc_size;
  double sub_eth_size;
  double sub_sol_size;
  std::string uid;

  bool grid_long;
  bool grid_short;
};

extern Config kConfig;

struct Ticker {
  std::string contract;
  double last;
};

struct Order {
  bool is_reduce_only;
  double size;
  int rule;
  double last_close_pnl;
  double price;
  double tp_price;
  double sl_price;
  std::string contract;
  std::string id;
  std::string tp_id;
  std::string sl_id;
  std::string side;
  std::string fill_price;
  std::string auto_size;
  std::string trigger_price;

  std::string start_time;
  std::string end_time;
  std::string status;  // NEW FILLED IDLE

  std::string position_side;
  std::string type;
};

struct Position {
  std::string position_side;
  double position_amt;
  double entry_price;
};

struct Contract {
  int order_size_min;
  std::string name;
  std::string leverage_min;
  std::string leverage_max;
  std::string quanto_multiplier;
  std::string mark_price;
  std::string order_price_round;
};

}  // namespace standx
