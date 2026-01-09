#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Poco/Runnable.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"
#include "data.h"
#include "standx_client.h"

namespace standx {

class Strategy : public Poco::Runnable {
 public:
  Strategy(std::shared_ptr<StandXClient> client);

  virtual ~Strategy();
  void run() override;

  void start();
  void stop();
  bool is_running() const { return thread_running_; }
  std::string get_inst_id() const { return inst_id_; }
  void init();

 private:
  bool update_position();
  void run_grid();
  void update_price();
  bool check_unfilled_orders();
  void check_filled_long_orders();
  void check_filled_short_orders();
  void run_long_grid();
  void run_short_grid();
  void delete_long_tp_orders();
  void delete_long_place_orders();
  void delete_short_tp_orders();
  void delete_short_place_orders();
  void count_long_reduce_size();
  void count_short_reduce_size();
  void init_long_place_orders();
  void init_short_place_orders();
  void init_long_tp_orders();
  void init_short_tp_orders();
  void make_long_place_orders();
  void make_short_place_orders();
  void make_long_tp_orders();
  void make_short_tp_orders();
  void init_parameters();
  void increase_long_position();
  void increase_short_position();
  void reset_daily_counters();

 private:
  bool thread_running_{false};
  bool grid_long_{false};
  bool grid_short_{false};

  std::string inst_id_;
  std::shared_ptr<Poco::Thread> thread_;
  std::shared_ptr<StandXClient> client_;

  Position long_pos_;
  Position short_pos_;

  double base_price_{0.0};
  double current_price_{0.0};
  double current_fix_long_price_{0.0};
  double current_fix_short_price_{0.0};
  double order_interval_{0.0};
  double grid_size_{0.1};
  int success_trades_total_{0};
  int success_trades_daily_{0};
  int last_reset_success_trades_day_{0};
  std::string order_price_round_;

  double long_reduce_size_{0.0};
  double short_reduce_size_{0.0};
  std::vector<Order> unfilled_orders_;
  std::map<std::string, Order> long_grid_order_list_;
  std::map<std::string, Order> short_grid_order_list_;
};

}  // namespace standx