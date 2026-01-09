/*!
* @file      tracer.h
* @version   v1.0.0
* @copyright (c) 2024
* @author    lanqsh@hotmail.com
* @date      2023-7-31
* @brief     logger
* @attention
*/
#pragma once

#include <cstdio>
#include <sstream>
#include <string>

#include "Poco/AutoPtr.h"
#include "Poco/ConsoleChannel.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/FileChannel.h"
#include "Poco/FormattingChannel.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/PatternFormatter.h"
#include "Poco/SimpleFileChannel.h"

namespace standx {

const std::string DEFAULT_LOGGER_NAME = "default";

/*!
* @class Tracer
* @brief support stream logger, macro INFO has one param
*        INFO_ has two param, first one for logger name.
*
* @attention MUST init before use
*
* @code
*
* standx::Tracer::init();
* standx::Tracer::init("demo", "/userdata/log/demo.log");

* int i_num = 10;
* double f_num = 10.0;
* std::string str = "test str";
* INFO(str << ", num = " << i_num << ", f_num = " << f_num);
* INFO_("demo", str << ", num = " << i_num << ", f_num = " << f_num);
*
* @endcode
*/

class Tracer {
public:
  static void init();
  static void init(const std::string& name, const std::string& path);
  static void init(const std::string& name, const std::string& path,
          const std::string& file_size);

  static void set_level(const std::string& level);
  static void set_level(const std::string& level, const std::string& name);
  static int get_level();
  static int get_level(const std::string& name);

private:
  static std::vector<std::string> loggers_;
};

} // namespace standx

#define LOGMSG(logger_name, message, level)                                   \
 do {                                                                        \
  try {                                                                     \
   if (Poco::Logger::has(logger_name)) {                                   \
    auto &logger = Poco::Logger::get(logger_name);                        \
    if (logger.getLevel() >= level) {                                     \
     std::stringstream ss;                                               \
     ss << message;                                                      \
     std::string filePath{__FILE__};                                     \
     size_t pos = filePath.find_last_of("/");                            \
     filePath = (pos == std::string::npos) ? filePath                    \
                        : filePath.substr(pos + 1);   \
     Poco::Message msg(logger.name(), ss.str(), level, filePath.c_str(), \
              __LINE__);                                        \
     logger.log(msg);                                                    \
    }                                                                     \
   } else {                                                                \
    perror("ERROR:logger should init before user\n");                     \
   }                                                                       \
  } catch (const Poco::Exception &e) {                                      \
   perror(e.what());                                                       \
  }                                                                         \
 } while (0);

#define TRACE_(logger_name, message) \
 LOGMSG(logger_name, message, Poco::Message::PRIO_TRACE)
#define DEBUG_(logger_name, message) \
 LOGMSG(logger_name, message, Poco::Message::PRIO_DEBUG)
#define INFO_(logger_name, message) \
 LOGMSG(logger_name, message, Poco::Message::PRIO_INFORMATION)
#define NOTICE_(logger_name, message) \
 LOGMSG(logger_name, message, Poco::Message::PRIO_NOTICE)
#define WARNING_(logger_name, message) \
 LOGMSG(logger_name, message, Poco::Message::PRIO_WARNING)
#define ERROR_(logger_name, message) \
 LOGMSG(logger_name, message, Poco::Message::PRIO_ERROR)
#define FATAL_(logger_name, message) \
 LOGMSG(logger_name, message, Poco::Message::PRIO_FATAL)

#define HEX_(logger_name, _msg, _buf, _len)                      \
 do {                                                           \
  auto &logger = Poco::Logger::get(logger_name);               \
  std::string msg = _msg;                                      \
  std::string pos = " [";                                      \
  pos = pos + __FILE__ + " " + std::to_string(__LINE__) + "]"; \
  msg += pos;                                                  \
  try {                                                        \
   logger.dump(msg, _buf, _len, Poco::Message::PRIO_DEBUG);   \
  } catch (const Poco::Exception &e) {                         \
   perror(e.what());                                          \
  }                                                            \
 } while (0);

#define TRACE(message) \
 LOGMSG(standx::DEFAULT_LOGGER_NAME, message, Poco::Message::PRIO_TRACE)
#define DEBUG(message) \
 LOGMSG(standx::DEFAULT_LOGGER_NAME, message, Poco::Message::PRIO_DEBUG)
#define INFO(message) \
 LOGMSG(standx::DEFAULT_LOGGER_NAME, message, Poco::Message::PRIO_INFORMATION)
#define NOTICE(message) \
 LOGMSG(standx::DEFAULT_LOGGER_NAME, message, Poco::Message::PRIO_NOTICE)
#define WARNING(message) \
 LOGMSG(standx::DEFAULT_LOGGER_NAME, message, Poco::Message::PRIO_WARNING)
#define ERROR(message) \
 LOGMSG(standx::DEFAULT_LOGGER_NAME, message, Poco::Message::PRIO_ERROR)
#define FATAL(message) \
 LOGMSG(standx::DEFAULT_LOGGER_NAME, message, Poco::Message::PRIO_FATAL)

#define HEX(_msg, _buf, _len) HEX_(standx::DEFAULT_LOGGER_NAME, _msg, _buf, _len)
