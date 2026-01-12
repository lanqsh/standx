#ifndef _DATA_H
#define _DATA_H

#include <cstdint>

#include "Poco/Timestamp.h"
#include "defines.h"

struct Config {
  float lever;
  float minAvailBal;
  std::string blackList;
  std::string whiteList;

  std::string apiKey;
  std::string secretKey;

  std::string logName;
  std::string logSize;
  std::string logLevel;

  std::string barkServer;

  float subBtcSize;
  float subEthSize;
  float subSolSize;
  std::string uid;

  bool gridLong;
  bool gridShort;
};

extern Config kConfig;
struct Ticker {
  std::string contract;
  float last;
};

struct Order {
  bool is_reduce_only{false};
  float size;
  int rule;
  float last_close_pnl;
  float price;
  float tp_price;
  float sl_price;
  std::string contract;
  std::string id;
  std::string tpId;
  std::string slId;
  std::string side;
  std::string fill_price;
  std::string auto_size;
  std::string trigger_price;

  std::string startTime;
  std::string endTime;
  std::string status;  // NEW FILLED IDLE

  std::string positionSide;
  std::string type;
};

struct Position {
  std::string positionSide;
  float positionAmt;
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

#endif
