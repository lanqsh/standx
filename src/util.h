#ifndef STANDX_SRC_UTIL_H_
#define STANDX_SRC_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "Poco/LocalDateTime.h"
#include "Poco/Thread.h"
#include "data.h"

#define THIS_YEAR Poco::LocalDateTime().year()
#define THIS_MONTH Poco::LocalDateTime().month()
#define THIS_DAY Poco::LocalDateTime().day()
#define THIS_HOUR Poco::LocalDateTime().hour()
#define THIS_MINUTE Poco::LocalDateTime().minute()
#define THIS_SECOND Poco::LocalDateTime().second()
#define THIS_MILLISEC Poco::LocalDateTime().millisecond()
#define SLEEP_MS(ms) Poco::Thread::sleep(ms)

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
    const T& enumValue, const int intValue) {
  return static_cast<int>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
    const T& enumValue, const uint32_t intValue) {
  return static_cast<uint32_t>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
    const uint32_t intValue, const T& enumValue) {
  return static_cast<uint32_t>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, std::ostream&>::type operator<<(
    std::ostream& os, T value) {
  return os << static_cast<typename std::underlying_type<T>::type>(value);
}

float SafeStof(const std::string& str);

std::string SafeFtos(float value, int places);

std::string BuildClOrdId(const std::string& inst_id, float price);

bool AreFloatsEqual(float a, float b, float epsilon = 1e-6);

std::string AdjustDecimalPlaces(float num, const std::string& epsilon);

std::string ConvertRemark(const std::string& remark);

void SendMessage(const std::string& message, bool force = false);

#endif  // STANDX_SRC_UTIL_H_