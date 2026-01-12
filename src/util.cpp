#include "util.h"

#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>

#include "Poco/LocalDateTime.h"
#include "Poco/Thread.h"
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
std::string hexEncode(const unsigned char* data, size_t length) {
  std::ostringstream hexStream;
  for (size_t i = 0; i < length; ++i) {
    hexStream << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
  }
  return hexStream.str();
}

std::string sha512(const std::string& input) {
  unsigned char hash[SHA512_DIGEST_LENGTH];
  SHA512(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(),
         hash);
  return hexEncode(hash, SHA512_DIGEST_LENGTH);
}

std::string generateSignature(const std::string& key, const std::string& data) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  HMAC(EVP_sha256(), key.c_str(), key.length(), (unsigned char*)data.c_str(),
       data.length(), digest, NULL);

  std::stringstream ss;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  }
  return ss.str();
}

std::string getTimestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_seconds = system_clock::to_time_t(now);
  std::tm tm = {};

  localtime_r(&now_seconds, &tm);
  auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

uint64_t safeStoll(const std::string& str) {
  if (str.empty()) return 0;

  try {
    return std::stoull(str);
  } catch (...) {
    ERROR("safeStoi error: " << str);
    return 0;
  }
}

int safeStoi(const std::string& str) {
  if (str.empty()) return 0;

  try {
    return std::stoi(str);
  } catch (...) {
    ERROR("safeStoi error: " << str);
    return 0;
  }
}

float safeStof(const std::string& str) {
  if (str.empty()) return 0.0f;

  try {
    return std::stof(str);
  } catch (...) {
    ERROR("safeStof error: " << str);
    return 0.0f;
  }
}

std::string safeFtos(float value, int places) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(places);
  oss << value;
  return oss.str();
}

bool areFloatsEqual(float a, float b, float epsilon) {
  return std::fabs(a - b) < epsilon;
}

std::string adjustDecimalPlaces(float num, const std::string& epsilon) {
  float epsilon_float = safeStof(epsilon);
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

std::string convertRemark(const std::string& remark) {
  std::string res;
  for (int i = 0; i < remark.size(); ++i) {
    char c = remark.at(i);
    auto it = kEscapingMap.find(c);
    if (it != kEscapingMap.end()) {
      res += kEscapingMap[c];
    } else {
      res += remark.at(i);
    }
  }
  return res;
}

void sendMessage(const std::string& message, bool force) {
  std::string endpoint = "curl -s -o /dev/null " + kConfig.barkServer;
  std::string ring = "?level=critical&volume=1";

  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm* local_time = std::localtime(&now_time);
  int current_hour = local_time->tm_hour;
  if (!force && current_hour < 8) {
    ring = "";
  }

  std::string cmd = convertRemark(message);
  cmd = endpoint + cmd + ring;
  int result = system(cmd.c_str());
  (void)result;
  NOTICE("Send message: " << cmd);
}