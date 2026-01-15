#ifndef _STRATEGY_H
#define _STRATEGY_H

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "Poco/Runnable.h"
#include "Poco/Thread.h"
#include "Poco/Timestamp.h"
#include "data.h"
#include "standx_client.h"
#include "tracer.h"

using standx::StandXClient;

class Strategy : public Poco::Runnable {
 public:
  Strategy(std::shared_ptr<StandXClient> client);

  virtual ~Strategy();
  void run() override;

  void start();
  void stop();
  bool isRunning() { return thread_->isRunning(); }
  std::string GetInstId() { return instId_; }
  void Init();

 private:
  bool UpdatePosition();
  void RunGrid();
  void UpdatePrice();
  bool CheckUnfilledOrders();
  void CheckFilledLongOrders();
  void CheckFilledShortOrders();
  void RunLongGrid();
  void RunShortGrid();
  void DeleteLongTpOrders();
  void DeleteLongPlaceOrders();
  void DeleteShortTpOrders();
  void DeleteShortPlaceOrders();
  void CountLongReduceSize();
  void CountShortReduceSize();
  void InitLongPlaceOrders();
  void InitShortPlaceOrders();
  void InitLongTpOrders();
  void InitShortTpOrders();
  void MakeLongPlaceOrders();
  void MakeShortPlaceOrders();
  void MakeLongTpOrders();
  void MakeShortTpOrders();
  void InitParameters();
  void IncreaseLongPosition();
  void IncreaseShortPosition();
  void ResetDailyCounters();
  void SyncPlacedOrderId(Order &order);
  void SyncTpOrderId(Order &order);

 private:
  bool thread_running_{false};
  bool grid_long_{false};
  bool grid_short_{false};

  std::string instId_;
  std::shared_ptr<Poco::Thread> thread_;
  std::shared_ptr<StandXClient> client_;

  Position long_pos_;
  Position short_pos_;

  float base_price_{0.0};
  float current_price_{0.0};
  float current_fix_long_price_{0.0};
  float current_fix_short_price_{0.0};
  float order_interval_{0.0};
  float grid_size_{0.1};
  int success_trades_total_{0};
  int success_trades_daily_{0};
  int last_reset_success_trades_day_{0};
  std::string order_price_round_;

  float long_reduce_size_{0.0};
  float short_reduce_size_{0.0};
  std::list<Order> unfilled_orders_;
  std::map<std::string, Order> long_grid_order_list_;
  std::map<std::string, Order> short_grid_order_list_;
};

#endif