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

#include "Poco/DateTime.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Timestamp.h"
#include "Poco/Timezone.h"
#include "tracer.h"
#include "util.h"

using standx::StandXClient;
static int s_win_cnt = 0;
static int s_lose_cnt = 0;
static float s_pnl = 0.0;
Strategy::Strategy(std::shared_ptr<StandXClient> client) : client_(client) {
  Init();
}

Strategy::~Strategy() { stop(); }

void Strategy::start() {
  thread_ = std::make_shared<Poco::Thread>();
  thread_->setName(instId_.substr(0, 3));
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

void Strategy::Init() {
  UpdatePosition();
  UpdatePrice();
  InitParameters();
  CheckUnfilledOrders();
  if (grid_long_) {
    InitLongPlaceOrders();
    InitLongTpOrders();
  }
  if (grid_short_) {
    InitShortPlaceOrders();
    InitShortTpOrders();
  }
}

void Strategy::InitParameters() {
  instId_ = client_->getInstId();
  order_price_round_ = safeFtos(PRICE_ACCURACY_FLOAT, PRICE_ACCURACY_INT);

  grid_long_ = kConfig.gridLong;
  grid_short_ = kConfig.gridShort;
  base_price_ = current_price_;
  grid_size_ = DEFAULT_CONTRACT_SIZE;
  order_interval_ = 0.1;

  Poco::DateTime now;
  last_reset_success_trades_day_ = now.day();

  if (instId_ == "BTC-USD") {
    grid_size_ = kConfig.subBtcSize;
    base_price_ = 100000;
    order_interval_ = 100;
  } else if (instId_ == "ETH-USD") {
    grid_size_ = kConfig.subEthSize;
    base_price_ = 4000;
    order_interval_ = 5;
  } else if (instId_ == "SOL-USD") {
    grid_size_ = kConfig.subSolSize;
    base_price_ = 200;
    order_interval_ = 0.25;
  }
}

bool Strategy::UpdatePosition() {
  std::vector<Position> positions_list;
  if (!client_->positions(positions_list)) {
    ERROR("Get position failed: " << instId_);
    return false;
  }

  for (auto& pos : positions_list) {
    DEBUG("Update Postion mode: " << pos.positionSide
                                  << ", size: " << pos.positionAmt);
    if (pos.positionSide == "LONG") {
      long_pos_ = pos;
    } else if (pos.positionSide == "SHORT") {
      short_pos_ = pos;
    }
  }
  return true;
}

void Strategy::UpdatePrice() {
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

void Strategy::RunGrid() {
  if (!CheckUnfilledOrders()) {
    return;
  }
  if (grid_long_) {
    RunLongGrid();
  }
  if (grid_short_) {
    RunShortGrid();
  }
}

void Strategy::run() {
  INFO("Strategy start running " << instId_);
  while (thread_running_) {
    ResetDailyCounters();
    UpdatePrice();
    UpdatePosition();
    RunGrid();
  }
  INFO("Strategy stop running " << instId_);
}

bool Strategy::CheckUnfilledOrders() {
  unfilled_orders_.clear();
  if (!client_->unfilledOrders(unfilled_orders_)) {
    ERROR("Failed to get unfilled orders");
    return false;
  }

  return true;
}

void Strategy::CheckFilledLongOrders() {
  // pending buy orders FILLED check
  for (auto it = long_grid_order_list_.rbegin();
       it != long_grid_order_list_.rend(); ++it) {
    Order& order = it->second;
    if (order.status != "NEW" && order.status != "PARTIALLY_FILLED" &&
        order.status != "FILLED_OPEN_IMMEDIATE") {
      continue;
    }

    bool tp = false;
    if (order.status == "FILLED_OPEN_IMMEDIATE") {
      tp = true;
    } else if (client_->detail(order)) {
      INFO("Check Filled place order: " << order.price << ", key: " << it->first
                                        << ", order.id: " << order.id
                                        << ", status: " << order.status);
      if (order.status == "FILLED") {
        tp = true;
        NOTICE("TRADE long place order FILLED: " << it->first
                                                 << ", price: " << order.price);
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

    if (tp) {
      int try_count = 10;
      for (int i = 0; i < try_count; ++i) {
        float tp_price =
            std::max(current_fix_long_price_, order.price) + order_interval_;
        order.size = grid_size_;
        order.tp_price = tp_price;
        order.side = "SELL";
        order.positionSide = "LONG";
        order.type = "LIMIT";
        DEBUG("TRADE Calculated tp_price: "
              << tp_price << ", current_fix_long_price_: "
              << current_fix_long_price_ << ", order.price: " << order.price
              << ", order_interval_: " << order_interval_);

        if (!client_->tpOrder(order)) {
          UpdatePrice();
        } else {
          SyncTpOrderId(order);
          DEBUG("TRADE Place TP order ok for "
                << it->first << " " << order.price << " " << order_interval_
                << " " << current_fix_long_price_ << ", tp_price: " << tp_price
                << ", tp id: " << order.tpId);
          break;
        }
      }
    }
  }

  // pending tp orders FILLED check
  for (auto it = long_grid_order_list_.begin();
       it != long_grid_order_list_.end(); ++it) {
    Order& order = it->second;
    if (order.status != "FILLED_CLOSE_WAIT" && order.status != "FILLED_CLOSE_IMMEDIATE") {
      continue;
    }

    Order tmp = it->second;
    tmp.id = tmp.tpId;
    bool tp = false;
    if (order.status == "FILLED_CLOSE_IMMEDIATE") {
      tp = true;
    } else if (client_->detail(tmp)) {
      INFO("Check Filled tp order: " << tmp.price << ", key: " << it->first
                                     << ", tmp.id: " << tmp.id
                                     << ", status: " << tmp.status);
      if (tmp.status == "FILLED") {
        tp = true;
      } else if (tmp.status == "FAILED") {
        ERROR("tp order failed: " << it->first);
        long_grid_order_list_.erase(it);
        break;
      } else if (tmp.status == "NEW") {
        float tp_price =
            std::max(current_fix_long_price_, order.price) + order_interval_;
        if (tmp.tp_price > tp_price + PRICE_ACCURACY_FLOAT && order.price > 0) {
          DEBUG("TRADE update tp at: " << order.tp_price << " " << tp_price);
          order.size = grid_size_;
          order.tp_price = tp_price;
          order.side = "SELL";
          order.positionSide = "LONG";
          order.type = "LIMIT";
          if (!client_->tpOrder(order)) {
            NOTICE("Failed to update long TP order for " << it->first);
            continue;
          } else {
            client_->cancelOrder(tmp.id);
            SyncTpOrderId(order);
            NOTICE("Updating long TP order ok for "
                   << it->first << " price: " << tmp.tp_price << " " << tp_price
                   << "id: " << tmp.id << " " << order.tpId);
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

    if (tp) {
      ++success_trades_total_;
      ++success_trades_daily_;
      NOTICE("TRADE long  tp success: " << success_trades_total_ << " "
                                        << it->first << " <-> "
                                        << it->second.tp_price);
      it->second.status = "IDLE";
      continue;
    }
  }
}

void Strategy::CheckFilledShortOrders() {
  for (auto it = short_grid_order_list_.begin();
       it != short_grid_order_list_.end(); ++it) {
    Order& order = it->second;
    if (order.status != "NEW" && order.status != "PARTIALLY_FILLED" &&
        order.status != "FILLED_OPEN_IMMEDIATE") {
      continue;
    }

    bool tp = false;
    if (order.status == "FILLED_OPEN_IMMEDIATE") {
      tp = true;
    } else if (client_->detail(order)) {
      INFO("Check Filled place order: " << order.price << ", key: " << it->first
                                        << ", order.id: " << order.id
                                        << ", status: " << order.status);
      if (order.status == "FILLED") {
        NOTICE("TRADE short place order FILLED: " << it->first << ", price: "
                                                  << order.price);
        tp = true;
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

    if (tp) {
      int try_count = 10;
      for (int i = 0; i < try_count; ++i) {
        float tp_price =
            std::min(current_fix_short_price_, order.price) - order_interval_;
        order.size = grid_size_;
        order.tp_price = tp_price;
        order.side = "BUY";
        order.positionSide = "SHORT";
        order.type = "LIMIT";
        DEBUG("TRADE Placing short tp order at price: " << order.tp_price);
        if (!client_->tpOrder(order)) {
          UpdatePrice();
        } else {
          SyncTpOrderId(order);
          DEBUG("TRADE Place TP order ok for "
                << it->first << " " << order.price << " " << order_interval_
                << " " << current_fix_short_price_ << ", tp_price: " << tp_price
                << ", tp id: " << order.tpId);
          break;
        }
      }
    }
  }

  for (auto it = short_grid_order_list_.rbegin();
       it != short_grid_order_list_.rend(); ++it) {
    Order& order = it->second;
    if (order.status != "FILLED_CLOSE_WAIT" && order.status != "FILLED_CLOSE_IMMEDIATE") {
      continue;
    }

    Order tmp = it->second;
    tmp.id = tmp.tpId;
    bool tp = false;
    if (order.status == "FILLED_CLOSE_IMMEDIATE") {
      tp = true;
    } else if (client_->detail(tmp)) {
      INFO("Check Filled tp order: " << tmp.price << ", key: " << it->first
                                     << ", tmp.id: " << tmp.id
                                     << ", status: " << tmp.status);
      if (tmp.status == "FILLED") {
        tp = true;
      } else if (tmp.status == "FAILED") {
        ERROR("tp order failed: " << it->first);
        auto it_to_erase = std::prev(it.base());
        short_grid_order_list_.erase(it_to_erase);
        break;
      } else if (tmp.status == "NEW") {
        float tp_price =
            std::min(current_fix_short_price_, order.price) - order_interval_;
        if (tmp.tp_price < tp_price - PRICE_ACCURACY_FLOAT && order.price > 0) {
          DEBUG("TRADE update tp at: " << order.tp_price << " " << tp_price);
          order.size = grid_size_;
          order.tp_price = tp_price;
          order.side = "BUY";
          order.positionSide = "SHORT";
          order.type = "LIMIT";
          if (!client_->tpOrder(order)) {
            NOTICE("Failed to place TP order for " << it->first);
            continue;
          } else {
            client_->cancelOrder(tmp.id);
            SyncTpOrderId(order);
            NOTICE("Updating short TP order ok for "
                   << it->first << " price: " << tmp.tp_price << " " << tp_price
                   << "id: " << tmp.id << " " << order.tpId);
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
    if (tp) {
      ++success_trades_total_;
      ++success_trades_daily_;
      NOTICE("TRADE short tp success: " << success_trades_total_ << " "
                                        << it->first << " <-> "
                                        << it->second.tp_price);
      it->second.status = "IDLE";
      continue;
    }
  }
}

void Strategy::RunLongGrid() {
  DeleteLongTpOrders();
  DeleteLongPlaceOrders();
  CountLongReduceSize();
  CheckFilledLongOrders();
  MakeLongPlaceOrders();
  MakeLongTpOrders();
}

void Strategy::RunShortGrid() {
  DeleteShortTpOrders();
  DeleteShortPlaceOrders();
  CountShortReduceSize();
  CheckFilledShortOrders();
  MakeShortPlaceOrders();
  MakeShortTpOrders();
}

void Strategy::DeleteLongTpOrders() {
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

void Strategy::DeleteLongPlaceOrders() {
  for (auto it = unfilled_orders_.begin(); it != unfilled_orders_.end();) {
    auto& order = *it;
    if (!order.is_reduce_only && order.size == grid_size_ &&
        order.price <
            current_fix_long_price_ - order_interval_ * ORDER_NUM * 2) {
      client_->cancelOrder(order.id);
      auto price_str = adjustDecimalPlaces(order.price, order_price_round_);
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

void Strategy::DeleteShortTpOrders() {
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

void Strategy::DeleteShortPlaceOrders() {
  for (auto it = unfilled_orders_.begin(); it != unfilled_orders_.end();) {
    auto& order = *it;
    if (!order.is_reduce_only && order.size == grid_size_ &&
        order.price >
            current_fix_short_price_ + order_interval_ * ORDER_NUM * 2) {
      client_->cancelOrder(order.id);
      auto price_str = adjustDecimalPlaces(order.price, order_price_round_);
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

void Strategy::CountLongReduceSize() {
  long_reduce_size_ = std::accumulate(
      unfilled_orders_.begin(), unfilled_orders_.end(), 0.0f,
      [](float sum, const auto& order) {
        return (order.is_reduce_only && order.positionSide == "LONG")
                   ? sum + order.size
                   : sum;
      });
}

void Strategy::CountShortReduceSize() {
  short_reduce_size_ = std::accumulate(
      unfilled_orders_.begin(), unfilled_orders_.end(), 0.0f,
      [](float sum, const auto& order) {
        return (order.is_reduce_only && order.positionSide == "SHORT")
                   ? sum + fabs(order.size)
                   : sum;
      });
}

void Strategy::InitLongPlaceOrders() {
  for (auto& order : unfilled_orders_) {
    if (!order.is_reduce_only && order.positionSide == "LONG") {
      auto price_str = adjustDecimalPlaces(order.price, order_price_round_);
      if (long_grid_order_list_.find(price_str) ==
          long_grid_order_list_.end()) {
        NOTICE("Init place long order not in grid list, price: "
               << order.price << ", price_str: " << price_str);
        long_grid_order_list_[price_str] = order;
      }
    }
  }
}

void Strategy::InitShortPlaceOrders() {
  for (auto& order : unfilled_orders_) {
    if (!order.is_reduce_only && order.positionSide == "SHORT") {
      auto price_str = adjustDecimalPlaces(order.price + order_interval_,
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

void Strategy::InitLongTpOrders() {
  for (auto& order : unfilled_orders_) {
    if (order.is_reduce_only && order.positionSide == "LONG") {
      auto price_str = adjustDecimalPlaces(order.price - order_interval_,
                                           order_price_round_);
      if (long_grid_order_list_.find(price_str) ==
          long_grid_order_list_.end()) {
        NOTICE("Init tp long order not in grid list, price: "
               << order.price << ", price_str: " << price_str);
        order.status = "FILLED";
        order.tpId = order.id;
        order.tp_price = order.price;
        order.price = 0;
        long_grid_order_list_[price_str] = order;
      }
    }
  }
}

void Strategy::InitShortTpOrders() {
  for (auto& order : unfilled_orders_) {
    if (order.is_reduce_only && order.positionSide == "SHORT") {
      auto price_str = adjustDecimalPlaces(order.price + order_interval_,
                                           order_price_round_);
      if (short_grid_order_list_.find(price_str) ==
          short_grid_order_list_.end()) {
        NOTICE("Init tp short order not in grid list, price: "
               << order.price << ", price_str: " << price_str);
        order.status = "FILLED";
        order.tpId = order.id;
        order.tp_price = order.price;
        order.price = 0;
        short_grid_order_list_[price_str] = order;
      }
    }
  }
}

void Strategy::MakeLongPlaceOrders() {
  for (int i = 0; i < ORDER_NUM; ++i) {
    float place_price = current_fix_long_price_ - order_interval_ * (i);
    auto place_price_str = adjustDecimalPlaces(place_price, order_price_round_);
    if (current_price_ - place_price < order_interval_ * 0.5) continue;

    bool place_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          return !order.is_reduce_only && order.positionSide == "LONG" &&
                 areFloatsEqual(order.price, place_price, PRICE_ACCURACY_FLOAT);
        });

    if (place_order_exists) {
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
      order.positionSide = "LONG";
      order.type = "LIMIT";
      order.price = place_price;
      order.size = grid_size_;
      order.status = "NEW";
      DEBUG("TRADE Making long place order at price: " << place_price);
      if (client_->placeOrder(order)) {
        SyncPlacedOrderId(order);
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

void Strategy::MakeShortPlaceOrders() {
  for (int i = 0; i < ORDER_NUM; ++i) {
    float place_price = current_fix_long_price_ + order_interval_ * (i);
    auto place_price_str = adjustDecimalPlaces(place_price, order_price_round_);
    if (place_price - current_price_ < order_interval_ * 0.5) continue;

    bool place_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          return !order.is_reduce_only && order.positionSide == "SHORT" &&
                 areFloatsEqual(order.price, place_price, PRICE_ACCURACY_FLOAT);
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
      order.positionSide = "SHORT";
      order.type = "LIMIT";
      order.price = place_price;
      order.size = grid_size_;
      order.status = "NEW";
      DEBUG("TRADE Making short place order at price: " << place_price);
      if (client_->placeOrder(order)) {
        SyncPlacedOrderId(order);
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

void Strategy::SyncPlacedOrderId(Order& order) {
  DEBUG("Sync placed order, price: " << order.price << ", side: " << order.side
                                     << ", positionSide: "
                                     << order.positionSide);
  for (int i = 0; i < 5; ++i) {
    SLEEP_MS(1000);
    CheckUnfilledOrders();
    for (auto& u : unfilled_orders_) {
      DEBUG("Sync placed order, u price: "
            << u.price << ", side: " << u.side << ", status: " << u.status
            << ", positionSide: " << u.positionSide);
      if (u.side == order.side && u.status == "NEW" &&
          areFloatsEqual(u.price, order.price, PRICE_ACCURACY_FLOAT)) {
        order.id = u.id;
        order.status = "NEW";
        DEBUG("Synced placed order with unfilled list, price: "
              << order.price << ", id: " << order.id);
        return;
      }
    }
  }
  order.status = "FILLED_OPEN_IMMEDIATE";
  DEBUG("Placed order not found in unfilled list, mark FILLED, price: "
        << order.price);
}

void Strategy::SyncTpOrderId(Order& order) {
  DEBUG("Sync tp order, price: " << order.price << ", side: " << order.side
                                 << ", positionSide: " << order.positionSide
                                 << ", tp_price: " << order.tp_price);
  for (int i = 0; i < 5; ++i) {
    SLEEP_MS(1000);
    CheckUnfilledOrders();
    for (auto& u : unfilled_orders_) {
      DEBUG("Sync tp order, u price: " << u.price << ", side: " << u.side
                                       << ", status: " << u.status
                                       << ", positionSide: " << u.positionSide);
      if (u.is_reduce_only && u.positionSide == order.positionSide &&
          areFloatsEqual(u.price, order.tp_price, PRICE_ACCURACY_FLOAT)) {
        order.tpId = u.id;
        order.status = "FILLED_CLOSE_WAIT";
        DEBUG("Synced TP order with unfilled list, tp_price: "
              << order.tp_price << ", tpId: " << order.tpId);
        return;
      }
    }
  }
  order.status = "FILLED_CLOSE_IMMEDIATE";
  DEBUG("TP order not found in unfilled list, mark FILLED, tp_price: "
        << order.tp_price);
}

void Strategy::MakeLongTpOrders() {
  int num = 5;
  for (int i = 0; i < num; ++i) {
    if (long_pos_.positionAmt - long_reduce_size_ < grid_size_) {
      IncreaseLongPosition();
      DEBUG("Insufficient long position size");
      break;
    }

    float tp_price = current_fix_long_price_ + order_interval_ * (i + num);
    auto tp_price_str =
        adjustDecimalPlaces(tp_price - order_interval_, order_price_round_);

    bool tp_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          return order.is_reduce_only && order.positionSide == "LONG" &&
                 areFloatsEqual(order.price, tp_price, PRICE_ACCURACY_FLOAT);
        });

    if (tp_order_exists) {
      continue;
    }

    Order order;
    order.side = "SELL";
    order.positionSide = "LONG";
    order.type = "LIMIT";
    order.tp_price = tp_price;
    order.size = grid_size_;

    auto it = long_grid_order_list_.find(tp_price_str);
    DEBUG("TRADE Placing long tp order at price: "
          << tp_price << ", key: " << tp_price_str
          << ", current_price_: " << current_price_);
    if (client_->tpOrder(order)) {
      SyncTpOrderId(order);
      long_reduce_size_ += grid_size_;
      if (it != long_grid_order_list_.end()) {
        it->second.tpId = order.tpId;
        DEBUG("Update place long tpId for price: " << tp_price_str
                                                   << ", tpId: " << order.tpId);
      } else {
        long_grid_order_list_[tp_price_str] = order;
        DEBUG("Update insert long tpId for price: "
              << tp_price_str << ", tpId: " << order.tpId);
      }

      NOTICE("TRADE Place TP order: " << order.contract << " " << order.tpId
                                      << " " << tp_price_str << " "
                                      << order.tp_price);
    } else {
      ERROR("Failed to place long TP order");
    }
  }
}

void Strategy::MakeShortTpOrders() {
  int num = 5;
  for (int i = 0; i < num; ++i) {
    if (fabs(short_pos_.positionAmt) - short_reduce_size_ < grid_size_) {
      IncreaseShortPosition();
      DEBUG("Insufficient short position size");
      break;
    }

    float tp_price = current_fix_short_price_ - order_interval_ * (i + num);
    auto tp_price_str =
        adjustDecimalPlaces(tp_price + order_interval_, order_price_round_);

    bool tp_order_exists = std::any_of(
        unfilled_orders_.begin(), unfilled_orders_.end(),
        [&](const auto& order) {
          return order.is_reduce_only && order.positionSide == "SHORT" &&
                 areFloatsEqual(order.price, tp_price, PRICE_ACCURACY_FLOAT);
        });

    if (tp_order_exists) {
      continue;
    }

    Order order;
    order.side = "BUY";
    order.positionSide = "SHORT";
    order.type = "LIMIT";
    order.tp_price = tp_price;
    order.size = grid_size_;

    auto it = short_grid_order_list_.find(tp_price_str);
    DEBUG("TRADE Placing short tp order at price: "
          << tp_price << ", key: " << tp_price_str
          << ", current_price_: " << current_price_);
    if (client_->tpOrder(order)) {
      SyncTpOrderId(order);
      short_reduce_size_ += grid_size_;
      if (it != short_grid_order_list_.end()) {
        it->second.tpId = order.tpId;
        DEBUG("Update place short tpId for price: "
              << tp_price_str << ", tpId: " << order.tpId);
      } else {
        short_grid_order_list_[tp_price_str] = order;
        DEBUG("Update insert short tpId for price: "
              << tp_price_str << ", tpId: " << order.tpId);
      }

      NOTICE("TRADE Place TP order: " << order.contract << " " << order.tpId
                                      << ", size: " << order.size
                                      << ", price: " << order.tp_price);
    } else {
      ERROR("Failed to place short TP order");
    }
  }
}

void Strategy::IncreaseLongPosition() {
  if (long_pos_.positionAmt < grid_size_ * ORDER_NUM * MAX_ORDER_NUM_FACTOR) {
    Order order;
    order.side = "BUY";
    order.positionSide = "LONG";
    order.type = "MARKET";
    order.price = 0;
    order.size = grid_size_ * ORDER_NUM;
    client_->placeOrder(order);
    NOTICE("Increase long position at " << current_price_);
    SLEEP_MS(1000);
  }
}

void Strategy::IncreaseShortPosition() {
  if (fabs(short_pos_.positionAmt) <
      grid_size_ * ORDER_NUM * MAX_ORDER_NUM_FACTOR) {
    Order order;
    order.side = "SELL";
    order.positionSide = "SHORT";
    order.type = "MARKET";
    order.price = 0;
    order.size = grid_size_ * ORDER_NUM;
    client_->placeOrder(order);
    NOTICE("Increase short position at " << current_price_);
    SLEEP_MS(1000);
  }
}

void Strategy::ResetDailyCounters() {
  Poco::DateTime now;
  int current_day = now.day();
  if (current_day != last_reset_success_trades_day_) {
    float availBal = 0;
    float totalBal = 0;
    if (!client_->balance(availBal, totalBal)) return;
    std::string msg = kConfig.uid + " " + instId_ + " binance trades " +
                      std::to_string(success_trades_daily_);
    msg += ", balance " + std::to_string(availBal) + " & " +
           std::to_string(totalBal);
    NOTICE(msg);
    sendMessage(msg);
    success_trades_daily_ = 0;
    last_reset_success_trades_day_ = current_day;
  }
}
