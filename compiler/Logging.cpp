#include <Logging.h>

namespace klang {

#define LOG_FUNC(level) \
  void Logger::level(const char* fmt, ...) { \
    va_list args; \
    va_start(args, fmt); \
    do_log(LOG_##level, fmt, args); \
    va_end(args); \
  }

LOG_FUNC(debug)
LOG_FUNC(info)
LOG_FUNC(warn)
LOG_FUNC(error)
LOG_FUNC(fatal)

Logger g_Logger(LOG_debug);

Logger& get_logger() {
  return g_Logger; 
}

} // namespace klang