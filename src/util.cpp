#include "util.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "tracer.h"

namespace {
std::map<char, std::string> kEscapingMap = {
    {' ', "%20"}, {'"', "%22"}, {'#', "%23"},  {'%', "%25"}, {'&', "%26"},
    {'(', "%28"}, {')', "%29"}, {'+', "%2B"},  {',', "%2C"}, {'/', "%2F"},
    {':', "%3A"}, {';', "%3B"}, {'<', "%3C"},  {'=', "%3D"}, {'>', "%3E"},
    {'?', "%3F"}, {'@', "%40"}, {'\\', "%5C"}, {'|', "%7C"}, {'`', "\\`"},
    {'*', "\\*"}, {'$', "\\$"}, {'[', "%5B"},  {']', "%5D"}, {'^', "%5E"},
    {'{', "%7B"}, {'}', "%7D"}, {'~', "%7E"}};
}

float SafeStof(const std::string& str) {
  if (str.empty()) return 0.0f;

  try {
    return std::stof(str);
  } catch (...) {
    ERROR("SafeStof error: " << str);
    return 0.0f;
  }
}

std::string SafeFtos(float value, int places) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(places);
  oss << value;
  return oss.str();
}

std::string BuildClOrdId(const std::string& inst_id, float price) {
  auto pos = inst_id.find('-');
  std::string symbol =
      (pos == std::string::npos) ? inst_id : inst_id.substr(0, pos);

  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm tm = {};
  localtime_r(&now_time, &tm);

  std::ostringstream ts;
  ts << std::put_time(&tm, "%Y%m%d%H%M%S");

  return symbol + "_" + SafeFtos(price, PRICE_ACCURACY_INT) + "_" + ts.str();
}

bool AreFloatsEqual(float a, float b, float epsilon) {
  return std::fabs(a - b) < epsilon;
}

std::string AdjustDecimalPlaces(float num, const std::string& epsilon) {
  float epsilon_float = SafeStof(epsilon);
  int precision = epsilon.size() - 2;

  num *= std::pow(10, precision);
  epsilon_float *= std::pow(10, precision);

  if (num / epsilon_float != 0) {
    num = std::round(num / epsilon_float) * epsilon_float;
  }

  num *= std::pow(10, -precision);

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(precision) << num;

  return oss.str();
}

std::string ConvertRemark(const std::string& remark) {
  std::string res;
  for (size_t i = 0; i < remark.size(); ++i) {
    char c = remark.at(i);
    auto it = kEscapingMap.find(c);
    if (it != kEscapingMap.end()) {
      res += it->second;
    } else {
      res += c;
    }
  }
  return res;
}

void SendMessage(const std::string& message, bool force) {
  if (kConfig.barkServer.empty()) {
    return;
  }
  std::string endpoint = "curl -s -o /dev/null " + kConfig.barkServer;
  std::string ring = "?level=critical&volume=1";

  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm* local_time = std::localtime(&now_time);
  int current_hour = local_time->tm_hour;
  if (!force && current_hour < 8) {
    ring = "";
  }

  std::string cmd = ConvertRemark(message);
  cmd = endpoint + cmd + ring;
  int result = system(cmd.c_str());
  (void)result;
  NOTICE("Send message: " << cmd);
}