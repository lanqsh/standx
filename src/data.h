#ifndef STANDX_SRC_DATA_H_
#define STANDX_SRC_DATA_H_

#include <cstdint>
#include <string>

#include "defines.h"

struct Config {
  float lever;
  float minAvailBal;
  std::string whiteList;

  std::string secretKey;
  std::string chain;

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
  float size{0.0};
  float price{0.0};
  float tp_price{0.0};

  std::string contract;
  std::string cl_ord_id;
  std::string tp_cl_ord_id;
  std::string side;
  std::string status;  // NEW FILLED IDLE

  std::string positionSide;
  std::string type;
};

struct Position {
  std::string positionSide;
  float positionAmt;
};

#endif  // STANDX_SRC_DATA_H_