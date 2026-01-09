#include "strategy.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>

#include "standx_client.h"

namespace standx {

Strategy::Strategy(std::shared_ptr<StandXClient> client) : client_(client) {
  init();
}

Strategy::~Strategy() { stop(); }

void Strategy::start() {
  thread_ = std::make_shared<Poco::Thread>();
  thread_->setName(inst_id_.substr(0, 3));
  if (!thread_running_ && thread_ != nullptr) {
    thread_->start(*this);
    thread_running_ = true;
  }
}

void Strategy::stop() {
  thread_running_ = false;
  if (thread_ != nullptr) {
    thread_->join();
    thread_ = nullptr;
  }
}

void Strategy::init() {
  update_position();
  update_price();
  init_parameters();
  check_unfilled_orders();
  if (grid_long_) {
    init_long_place_orders();
    init_long_tp_orders();
  }
  if (grid_short_) {
    init_short_place_orders();
    init_short_tp_orders();
  }
}

void Strategy::init_parameters() {
  inst_id_ = "ETH-USD";  // TODO: Get from client
  order_price_round_ = "0.01";

  grid_long_ = kConfig.grid_long;
  grid_short_ = kConfig.grid_short;
  base_price_ = current_price_;
  grid_size_ = DEFAULT_CONTRACT_SIZE;
  order_interval_ = 0.1;

  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time_t);
  last_reset_success_trades_day_ = tm.tm_mday;

  if (inst_id_ == "BTC-USD") {
    grid_size_ = 0.001;  // TODO: Load from config
    base_price_ = 100000;
  } else if (inst_id_ == "ETH-USD") {
    grid_size_ = 0.01;
    base_price_ = 4000;
  } else if (inst_id_ == "SOL-USD") {
    grid_size_ = 0.1;
    base_price_ = 200;
    order_interval_ = 0.05;
  }
}

bool Strategy::update_position() {
  // TODO: Implement using StandXClient::query_positions()
  std::cout << "Update position for: " << inst_id_ << std::endl;
  return true;
}

void Strategy::update_price() {
  // TODO: Implement using StandXClient::query_symbol_price()
  current_price_ = 3500.0;  // Placeholder
  if (pos.position_side == "LONG") {
    long_pos_ = pos;
  } else if (pos.position_side == "SHORT") {
    short_pos_ = pos;
  }
}
return true;
}  // namespace standx

void Strategy::update_price() {
  Ticker tk;
  if (client_->tickers(tk)) {
    current_price_ = tk.last;
    int price_int = current_price_ / order_interval_;
    current_fix_long_price_ = price_int * order_interval_;
    current_fix_short_price_ = current_fix_long_price_ + order_interval_;
    INFO("Current price: " << instId_ << " " << current_price_ << " "
                           << current_fix_long_price_ << " "
                           << current_fix_short_price_);
  } else {
    ERROR("Failed to get current price");
  }
}

void Strategy::run_grid() {
  if (!check_unfilled_orders()) {
    return;
  }
  if (grid_long_) {
    run_long_grid();
  }
  if (grid_short_) {
    run_short_grid();
  }
}

void Strategy::run() {
  INFO("Strategy start running " << instId_);
  while (thread_running_) {
    reset_daily_counters();
    update_price();
    update_position();
    run_grid();
  }
  INFO("Strategy stop running " << instId_);
}

bool Strategy::check_unfilled_orders() {
  unfilled_orders_.clear();
  if (!client_->unfilledOrders(unfilled_orders_)) {
    ERROR("Failed to get unfilled orders");
    return false;
  }

  return true;
}

void Strategy::check_filled_long_orders() {
  // pending buy orders FILLED check
  for (auto it = long_grid_order_list_.rbegin();
       it != long_grid_order_list_.rend(); ++it) {
    Order& order = it->second;
    if (order.status != "NEW" && order.status != "PARTIALLY_FILLED") {
      continue;
    }

    if (order.id.empty()) {
      ERROR("Order ID is empty: " << it->first);
      auto it_to_erase = std::prev(it.base());
      long_grid_order_list_.erase(it_to_erase);
      break;
    }

    if (client_->detail(order)) {
      INFO("Check Filled place order: " << order.price << ", key: " << it->first
                                        << ", order.id: " << order.id
                                        << ", status: " << order.status);
      if (order.status == "FILLED") {
        NOTICE("TRADE long place order FILLED: " << it->first
                                                 << ", price: " << order.price);
        int try_count = 10;
        for (int i = 0; i < try_count; ++i) {
          float tp_price =
              std::max(current_fix_long_price_, order.price) + order_interval_;
          DEBUG("Calculated tp_price: "
                << tp_price << ", current_fix_long_price_: "
                << current_fix_long_price_ << ", order.price: " << order.price
                << ", order_interval_: " << order_interval_);
          order.size = grid_size_;
          order.tp_price = tp_price;
          order.side = "SELL";
          order.position_side = "LONG";
          order.type = "LIMIT";

          if (!client_->tpOrder(order)) {
            UpdatePrice();
          } else {
            DEBUG("TRADE Place TP order ok for "
                  << it->first << " " << order.price << " " << order_interval_
                  << " " << current_fix_long_price_
                  << ", tp_price: " << tp_price << ", tp id: " << order.tp_id);
            break;
          }
        }
      } else if (order.status == "FAILED") {
        ERROR("place order failed: " << it->first);
        auto it_to_erase = std::prev(it.base());
        long_grid_order_list_.erase(it_to_erase);
        break;
      } else if (order.status == "NEW") {
        DEBUG("place order still NEW: " << it->first);
        break;
      } else if (order.status == "PARTIALLY_FILLED") {
        DEBUG("place order PARTIALLY_FILLED: " << it->first);
        break;
      } else if (order.status == "CANCELED") {
        DEBUG("place order CANCELED: " << it->first);
        order.status = "IDLE";
        break;
      } else {
        ERROR("place order failed: " << it->first << ", id: " << order.id
                                     << ", status: " << order.status);
        break;
      }
    }
  }

  // pending tp orders FILLED check
  for (auto it = long_grid_order_list_.begin();
       it != long_grid_order_list_.end(); ++it) {
    Order& order = it->second;
    if (order.status != "FILLED") {
      continue;
    }

    Order tmp = it->second;
    tmp.id = tmp.tp_id;
    if (client_->detail(tmp)) {
      INFO("Check Filled tp order: " << tmp.price << ", key: " << it->first
                                     << ", tmp.id: " << tmp.id
                                     << ", status: " << tmp.status);
      if (tmp.status == "FILLED") {
        ++success_trades_total_;
        ++success_trades_daily_;
        NOTICE("TRADE long  tp success: " << success_trades_total_ << " "
                                          << it->first << " <-> "
                                          << it->second.tp_price);
        it->second.status = "IDLE";
        continue;
      } else if (tmp.status == "FAILED") {
        ERROR("tp order failed: " << it->first);
        long_grid_order_list_.erase(it);
        break;
      } else if (tmp.status == "NEW") {
        float tp_price =
            std::max(current_fix_long_price_, order.price) + order_interval_;
        if (tmp.tp_price > tp_price + PRICE_ACCURACY_FLOAT) {
          order.size = grid_size_;
          order.tp_price = tp_price;
          order.side = "SELL";
          order.position_side = "LONG";
          order.type = "LIMIT";
          if (!client_->tpOrder(order)) {
            NOTICE("Failed to update long TP order for " << it->first);
            continue;
          } else {
            client_->cancelOrder(tmp.id);
            DEBUG("Updating long TP order ok for "
                  << it->first << " " << order.price << " " << order_interval_
                  << " " << current_fix_long_price_ << ", old tp price: "
                  << tmp.tp_price << ", new tp price: " << tp_price
                  << ", tp id: " << order.tp_id);
          }
        }
        DEBUG("tp order still NEW: " << it->first);
        break;
      } else if (tmp.status == "PARTIALLY_FILLED") {
        DEBUG("tp order PARTIALLY_FILLED: " << it->first);
        break;
      } else if (tmp.status == "CANCELED") {
        DEBUG("place order CANCELED: " << it->first);
        order.status = "IDLE";
        break;
      } else {
        ERROR("tp order failed: " << it->first << ", id: " << tmp.id
                                  << ", status: " << tmp.status);
        break;
      }
    }
  }
}

void Strategy::check_filled_short_orders() {
  for (auto it = short_grid_order_list_.begin();
       it != short_grid_order_list_.end(); ++it) {
    Order& order = it->second;
    if (order.status != "NEW" && order.status != "PARTIALLY_FILLED") {
      continue;
    }

    if (order.id.empty()) {
      ERROR("Order ID is empty: " << it->first);
      short_grid_order_list_.erase(it);
      break;
    }

    if (client_->detail(order)) {
      INFO("Check Filled place order: " << order.price << ", key: " << it->first
                                        << ", order.id: " << order.id
                                        << ", status: " << order.status);
      if (order.status == "FILLED") {
        NOTICE("TRADE short place order FILLED: " << it->first << ", price: "
                                                  << order.price);
        int try_count = 10;
        for (int i = 0; i < try_count; ++i) {
          float tp_price =
              std::min(current_fix_short_price_, order.price) - order_interval_;
          order.size = grid_size_;
          order.tp_price = tp_price;
          order.side = "BUY";
          order.position_side = "SHORT";
          order.type = "LIMIT";

          if (!client_->tpOrder(order)) {
            UpdatePrice();
          } else {
            DEBUG("TRADE Place TP order ok for "
                  << it->first << " " << order.price << " " << order_interval_
                  << " " << current_fix_long_price_
                  << ", tp_price: " << tp_price << ", tp id: " << order.tp_id);
            break;
          }
        }
      } else if (order.status == "FAILED") {
        ERROR("place order failed: " << it->first);
        short_grid_order_list_.erase(it);
        break;
      } else if (order.status == "NEW") {
        DEBUG("place order still NEW: " << it->first);
        break;
      } else if (order.status == "PARTIALLY_FILLED") {
        DEBUG("place order PARTIALLY_FILLED: " << it->first);
        break;
      } else if (order.status == "CANCELED") {
        DEBUG("place order CANCELED: " << it->first);
        order.status = "IDLE";
        break;
      } else {
        ERROR("place order failed: " << it->first << ", id: " << order.id
                                     << ", status: " << order.status);
        break;
      }
    }
  }

  for (auto it = short_grid_order_list_.rbegin();
       it != short_grid_order_list_.rend(); ++it) {
    Order& order = it->second;
    if (order.status != "FILLED") {
      continue;
    }

    Order tmp = it->second;
    tmp.id = tmp.tp_id;
    if (client_->detail(tmp)) {
      INFO("Check Filled tp order: " << tmp.price << ", key: " << it->first
                                     << ", tmp.id: " << tmp.id
                                     << ", status: " << tmp.status);
      if (tmp.status == "FILLED") {
        ++success_trades_total_;
        ++success_trades_daily_;
        NOTICE("TRADE short tp success: " << success_trades_total_ << " "
                                          << it->first << " <-> "
                                          << it->second.tp_price);
        it->second.status = "IDLE";
        continue;
      } else if (tmp.status == "FAILED") {
        ERROR("tp order failed: " << it->first);
        auto it_to_erase = std::prev(it.base());
        short_grid_order_list_.erase(it_to_erase);
        break;
      } else if (tmp.status == "NEW") {
        float tp_price =
            std::min(current_fix_short_price_, order.price) - order_interval_;
        if (tmp.tp_price < tp_price - PRICE_ACCURACY_FLOAT) {
          order.size = grid_size_;
          order.tp_price = tp_price;
          order.side = "BUY";
          order.position_side = "SHORT";
          order.type = "LIMIT";
          if (!client_->tpOrder(order)) {
            NOTICE("Failed to place TP order for " << it->first);
            continue;
          } else {
            client_->cancelOrder(tmp.id);
            DEBUG("Updating short TP order ok for "
                  << it->first << " " << order.price << " " << order_interval_
                  << " " << current_fix_short_price_ << ", old tp price: "
                  << tmp.tp_price << ", new tp price: " << tp_price
                  << ", tp id: " << order.tp_id);
          }
        }
        DEBUG("tp order still NEW: " << it->first);
        break;
      } else if (tmp.status == "PARTIALLY_FILLED") {
        DEBUG("tp order PARTIALLY_FILLED: " << it->first);
        break;
      } else if (tmp.status == "CANCELED") {
        DEBUG("tp order CANCELED: " << it->first);
        order.status = "IDLE";
        break;
      } else {
        ERROR("tp order failed: " << it->first << ", id: " << tmp.id
                                  << ", status: " << tmp.status);
        break;
      }
    }
  }
}

void Strategy::run_long_grid() {
  delete_long_tp_orders();
  delete_long_place_orders();
  count_long_reduce_size();
  check_filled_long_orders();
  make_long_place_orders();
  make_long_tp_orders();
}

void Strategy::run_short_grid() {
  delete_short_tp_orders();
  delete_short_place_orders();
  count_short_reduce_size();
  check_filled_short_orders();
  make_short_place_orders();
  make_short_tp_orders();
}

void Strategy::delete_long_tp_orders() {
  for (auto it = unfilled_orders_.begin(); it != unfilled_orders_.end();) {
    auto& order = *it;
    if (order.is_reduce_only && order.size == grid_size_ &&
        order.price >
            current_fix_long_price_ + order_interval_ * ORDER_NUM * 2) {
      client_->cancelOrder(order.id);
      DEBUG("Cancel long tp order " << order.contract << " " << order.id
                                    << ", price: " << order.price
                                    << ", current_price_: " << current_price_);
      it = unfilled_orders_.erase(it);
    } else {
      ++it;
    }
  }
}

void Strategy::delete_long_place_orders() {
  for (auto it = unfilled_orders_.begin(); it != unfilled_orders_.end();) {
    auto& order = *it;
    if (!order.is_reduce_only && order.size == grid_size_ &&
        order.price <
            current_fix_long_price_ - order_interval_ * ORDER_NUM * 2) {
      client_->cancelOrder(order.id);
      auto price_str = adjust_decimal_places(order.price, order_price_round_);
      auto itr = long_grid_order_list_.find(price_str);
      if (itr != long_grid_order_list_.end()) {
        itr->second.status = "IDLE";
        DEBUG("Erase long grid order list for price: "
              << order.price << ", key: " << price_str);
      }
      it = unfilled_orders_.erase(it);
      DEBUG("Cancel long place order too far price: "
            << order.price << ", current_price_: " << current_price_);
    } else {
      ++it;
    }
  }
}

void Strategy::delete_short_tp_orders() {
  for (auto it = unfilled_orders_.begin(); it != unfilled_orders_.end();) {
    auto& order = *it;
    if (order.is_reduce_only && order.size == grid_size_ &&
        order.price <
            current_fix_short_price_ - order_interval_ * ORDER_NUM * 2) {
      client_->cancelOrder(order.id);
      DEBUG("Cancel short tp order " << order.contract << " " << order.id
                                     << ", price: " << order.price
                                     << ", current_price_: " << current_price_);
      it = unfilled_orders_.erase(it);
    } else {
      ++it;
    }
  }
}

void Strategy::delete_short_place_orders() {
  for (auto it = unfilled_orders_.begin(); it != unfilled_orders_.end();) {
    auto& order = *it;
    if (!order.is_reduce_only && order.size == grid_size_ &&
        order.price >
            current_fix_short_price_ + order_interval_ * ORDER_NUM * 2) {
      client_->cancelOrder(order.id);
      auto price_str = adjust_decimal_places(order.price, order_price_round_);
      auto itr = short_grid_order_list_.find(price_str);
      if (itr != short_grid_order_list_.end()) {
        itr->second.status = "IDLE";
        DEBUG("Erase short grid order list for price: "
              << order.price << ", key: " << price_str);
      }
      it = unfilled_orders_.erase(it);
      DEBUG("Cancel short place order too far price: "
            << order.price << ", current_price_: " << current_price_);
    } else {
      ++it;
    }
  }
}

void Strategy::count_long_reduce_size() {
  long_reduce_size_ = std::accumulate(
      unfilled_orders_.begin(), unfilled_orders_.end(), 0.0f,
      [](float sum, const auto& order) {
        return (order.is_reduce_only && order.position_side == "LONG")
                   ? sum + order.size
                   : sum;
      });
}

void Strategy::count_short_reduce_size() {
  short_reduce_size_ = std::accumulate(
      unfilled_orders_.begin(), unfilled_orders_.end(), 0.0f,
      [](float sum, const auto& order) {
        return (order.is_reduce_only && order.position_side == "SHORT")
                   ? sum + fabs(order.size)
                   : sum;
      });
}

void Strategy::init_long_place_orders() {
  for (auto& order : unfilled_orders_) {
    if (!order.is_reduce_only && order.position_side == "LONG") {
      auto price_str = adjust_decimal_places(order.price, order_price_round_);
      if (long_grid_order_list_.find(price_str) ==
          long_grid_order_list_.end()) {
        NOTICE("Init place long order not in grid list, price: "
               << order.price << ", price_str: " << price_str);
        long_grid_order_list_[price_str] = order;
      }
    }
  }
}

void Strategy::init_short_place_orders() {
  for (auto& order : unfilled_orders_) {
    if (!order.is_reduce_only && order.position_side == "SHORT") {
      auto price_str = adjust_decimal_places(order.price + order_interval_,
                                             order_price_round_);
      if (short_grid_order_list_.find(price_str) ==
          short_grid_order_list_.end()) {
        NOTICE("Init place short order not in grid list, price: "
               << order.price << ", price_str: " << price_str);
        short_grid_order_list_[price_str] = order;
      }
    }
  }
}

void Strategy::init_long_tp_orders() {
  for (auto& order : unfilled_orders_) {
    if (order.is_reduce_only && order.position_side == "LONG") {
      auto price_str = adjust_decimal_places(order.price - order_interval_,
                                             order_price_round_);
      if (long_grid_order_list_.find(price_str) ==
          long_grid_order_list_.end()) {
        NOTICE("Init tp long order not in grid list, price: "
               << order.price << ", price_str: " << price_str);
        order.status = "FILLED";
        order.tp_id = order.id;
        order.tp_price = order.price;
        long_grid_order_list_[price_str] = order;
      }
    }
  }
}

void Strategy::init_short_tp_orders() {
  for (auto& order : unfilled_orders_) {
    if (order.is_reduce_only && order.position_side == "SHORT") {
      auto price_str = adjust_decimal_places(order.price, order_price_round_);
      if (short_grid_order_list_.find(price_str) ==
          short_grid_order_list_.end()) {
        NOTICE("Init tp short order not in grid list, price: "
               << order.price << ", price_str: " << price_str);
        order.status = "FILLED";
        order.tp_id = order.id;
        order.tp_price = order.price;
        short_grid_order_list_[price_str] = order;
      }
    }
  }
}

void Strategy::make_long_place_orders() {
  for (int i = 0; i < ORDER_NUM; ++i) {
    float place_price = current_fix_long_price_ - order_interval_ * (i);
    auto place_price_str =
        adjust_decimal_places(place_price, order_price_round_);
    if (current_price_ - place_price < order_interval_ * 0.5) continue;

    bool place_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          DEBUG("Checking unfilled order price: "
                << order.is_reduce_only
                << ", positionSide: " << order.position_side
                << ", place_price: " << order.price << " vs " << place_price);
          return !order.is_reduce_only && order.position_side == "LONG" &&
                 are_floats_equal(order.price, place_price,
                                  PRICE_ACCURACY_FLOAT);
        });

    if (place_order_exists) {
      DEBUG("place order exists " << place_price_str);
      continue;
    }

    bool place_order_idle = false;
    auto it = long_grid_order_list_.find(place_price_str);
    if (it == long_grid_order_list_.end()) {
      place_order_idle = true;
      DEBUG("place order not exist " << place_price_str);
    } else if (it->second.status == "IDLE") {
      place_order_idle = true;
      DEBUG("place order IDLE " << place_price_str);
    } else {
      DEBUG("place order found in grid list, status: "
            << it->second.status << ", price: " << it->second.price);
    }

    if (place_order_idle) {
      Order order;
      order.side = "BUY";
      order.position_side = "LONG";
      order.type = "LIMIT";
      order.price = place_price;
      order.size = grid_size_;
      order.status = "NEW";
      if (client_->placeOrder(order)) {
        long_grid_order_list_[place_price_str] = order;
        NOTICE("TRADE Place Long Order: "
               << order.contract << " " << order.id << ", size: " << order.size
               << ", key: " << place_price_str << ", price: " << order.price
               << ", current_price_: " << current_price_);
      } else {
        NOTICE("Failed to place long order");
      }
    }
  }
}

void Strategy::make_short_place_orders() {
  for (int i = 0; i < ORDER_NUM; ++i) {
    float place_price = current_fix_long_price_ + order_interval_ * (i);
    auto place_price_str =
        adjust_decimal_places(place_price, order_price_round_);
    if (place_price - current_price_ < order_interval_ * 0.5) continue;

    bool place_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          return !order.is_reduce_only && order.position_side == "SHORT" &&
                 are_floats_equal(order.price, place_price,
                                  PRICE_ACCURACY_FLOAT);
        });

    if (place_order_exists) {
      continue;
    }

    bool place_order_idle = false;
    auto it = short_grid_order_list_.find(place_price_str);
    if (it == short_grid_order_list_.end()) {
      place_order_idle = true;
      DEBUG("place order not exist " << place_price_str);
    } else if (it->second.status == "IDLE") {
      place_order_idle = true;
      DEBUG("place order IDLE " << place_price_str);
    } else {
      DEBUG("place order found in grid list, status: "
            << it->second.status << ", price: " << it->second.price);
    }

    if (place_order_idle) {
      Order order;
      order.side = "SELL";
      order.position_side = "SHORT";
      order.type = "LIMIT";
      order.price = place_price;
      order.size = grid_size_;
      order.status = "NEW";
      if (client_->placeOrder(order)) {
        short_grid_order_list_[place_price_str] = order;
        NOTICE("TRADE Place Short Order: "
               << order.contract << " " << order.id << ", size: " << order.size
               << ", key: " << place_price_str << ", price: " << order.price
               << ", current_price_: " << current_price_);
      } else {
        NOTICE("Failed to place short order");
      }
    }
  }
}

void Strategy::make_long_tp_orders() {
  for (int i = 0; i < 5; ++i) {
    if (long_pos_.position_amt - long_reduce_size_ < grid_size_) {
      increase_long_position();
      DEBUG("Insufficient long position size");
      break;
    }

    float tp_price =
        current_fix_long_price_ + order_interval_ * (i + ORDER_NUM);
    auto tp_price_str =
        adjust_decimal_places(tp_price - order_interval_, order_price_round_);

    bool tp_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          return order.is_reduce_only && order.position_side == "LONG" &&
                 are_floats_equal(order.price, tp_price, PRICE_ACCURACY_FLOAT);
        });

    if (tp_order_exists) {
      continue;
    }

    Order order;
    order.side = "SELL";
    order.position_side = "LONG";
    order.type = "LIMIT";
    order.tp_price = tp_price;
    order.size = grid_size_;

    auto it = long_grid_order_list_.find(tp_price_str);
    DEBUG("Placing long tp order at price: "
          << tp_price << ", key: " << tp_price_str
          << ", current_price_: " << current_price_);
    if (client_->tpOrder(order)) {
      long_reduce_size_ += grid_size_;
      if (it != long_grid_order_list_.end()) {
        it->second.tp_id = order.tp_id;
        DEBUG("Update place long tpId for price: " << tp_price_str
                                                   << ", tpId: " << order.tp_id);
      } else {
        order.status = "FILLED";
        long_grid_order_list_[tp_price_str] = order;
        DEBUG("Update insert long tpId for price: "
              << tp_price_str << ", tpId: " << order.tp_id);
      }

      NOTICE("TRADE Place TP order: " << order.contract << " " << order.tp_id
                                      << " " << tp_price_str << " "
                                      << order.tp_price);
    } else {
      ERROR("Failed to place long TP order");
    }
  }
}

void Strategy::make_short_tp_orders() {
  for (int i = 0; i < 5; ++i) {
    if (fabs(short_pos_.position_amt) - short_reduce_size_ < grid_size_) {
      increase_short_position();
      DEBUG("Insufficient short position size");
      break;
    }

    float tp_price =
        current_fix_short_price_ - order_interval_ * (i + ORDER_NUM);
    auto tp_price_str =
        adjust_decimal_places(tp_price + order_interval_, order_price_round_);

    bool tp_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          return order.is_reduce_only && order.position_side == "SHORT" &&
                 are_floats_equal(order.price, tp_price, PRICE_ACCURACY_FLOAT);
        });

    if (tp_order_exists) {
      continue;
    }

    Order order;
    order.side = "BUY";
    order.position_side = "SHORT";
    order.type = "LIMIT";
    order.tp_price = tp_price;
    order.size = grid_size_;

    auto it = short_grid_order_list_.find(tp_price_str);
    DEBUG("Placing short tp order at price: "
          << tp_price << ", key: " << tp_price_str
          << ", current_price_: " << current_price_);
    if (client_->tpOrder(order)) {
      short_reduce_size_ += grid_size_;
      if (it != short_grid_order_list_.end()) {
        it->second.tp_id = order.tp_id;
        DEBUG("Update place short tpId for price: "
              << tp_price_str << ", tpId: " << order.tp_id);
      } else {
        order.status = "FILLED";
        short_grid_order_list_[tp_price_str] = order;
        DEBUG("Update insert short tpId for price: "
              << tp_price_str << ", tpId: " << order.tp_id);
      }

      NOTICE("TRADE Place TP order: " << order.contract << " " << order.tp_id
                                      << ", size: " << order.size
                                      << ", price: " << order.tp_price);
    } else {
      ERROR("Failed to place short TP order");
    }
  }
}

void Strategy::increase_long_position() {
  if (long_pos_.position_amt < grid_size_ * ORDER_NUM * MAX_ORDER_NUM_FACTOR) {
    Order order;
    order.side = "BUY";
    order.position_side = "LONG";
    order.type = "MARKET";
    order.price = 0;
    order.size = grid_size_ * ORDER_NUM;
    client_->placeOrder(order);
    NOTICE("Increase long position at " << current_price_);
    SLEEP_MS(1000);
  }
}

void Strategy::increase_short_position() {
  if (fabs(short_pos_.position_amt) <
      grid_size_ * ORDER_NUM * MAX_ORDER_NUM_FACTOR) {
    Order order;
    order.side = "SELL";
    order.position_side = "SHORT";
    order.type = "MARKET";
    order.price = 0;
    order.size = grid_size_ * ORDER_NUM;
    client_->placeOrder(order);
    NOTICE("Increase short position at " << current_price_);
    SLEEP_MS(1000);
  }
}

void Strategy::reset_daily_counters() {
  Poco::DateTime now;
  int current_day = now.day();
  if (current_day != last_reset_success_trades_day_) {
    float availBal = 0;
    float totalBal = 0;
    if (!Binance::balance(availBal, totalBal)) return;
    std::string msg = kConfig.uid + " " + instId_ + " binance trades " +
                      std::to_string(success_trades_daily_);
    msg += ", balance " + std::to_string(availBal) + " & " +
           std::to_string(totalBal);
    NOTICE(msg);
    send_message(msg);
    success_trades_daily_ = 0;
    last_reset_success_trades_day_ = current_day;
  }
}
