#ifndef _LOGGING_H
#define _LOGGING_H

#include <mutex>
#include <iostream>
#include <fstream>
#include <cstdarg>

namespace klang {

enum LogLevel : int {
  LOG_debug = 0,
  LOG_info,
  LOG_warn,
  LOG_error,
  LOG_fatal,
};

class Logger {
public:
  Logger() : level_(LOG_info) {}
  Logger(LogLevel level) : level_(level) {}

  inline LogLevel level() const { return level_; }
  inline void set_level(LogLevel level) { level_ = level; }

  void debug(const char* fmt, ...);
  void info(const char* fmt, ...);
  void warn(const char* fmt, ...);
  void error(const char* fmt, ...);
  void fatal(const char* fmt, ...);

private:
  std::mutex mutex_;
  LogLevel level_;

  void do_log(LogLevel level, const char *fmt, va_list args) {
    if (level < level_) return;
    mutex_.lock();
    vfprintf(stderr, fmt, args);
    mutex_.unlock();
  }
};

Logger& get_logger();

#define LOG() get_logger()
#define DEBUG(fmt, ...) LOG().debug("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...) LOG().info("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt, ...) LOG().warn("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define ERROR(fmt, ...) LOG().error("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define FATAL(fmt, ...) LOG().fatal("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

} // namespace klang

#endif