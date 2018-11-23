/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef XLIB_LOG_H_
#define XLIB_LOG_H_

#include "string"
#include <sstream>
#include "xlib/time.h"

namespace xlib {

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

#define STRACE(fmt, ...) xlib::Log::Instance().SWrite(xlib::LOG_PRIORITY_TRACE, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define SDBG(fmt, ...)   xlib::Log::Instance().SWrite(xlib::LOG_PRIORITY_DEBUG, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define SINF(fmt, ...)   xlib::Log::Instance().SWrite(xlib::LOG_PRIORITY_INFO,  __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define SERR(fmt, ...)   xlib::Log::Instance().SWrite(xlib::LOG_PRIORITY_ERROR, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define SFATAL(fmt, ...) xlib::Log::Instance().SWrite(xlib::LOG_PRIORITY_FATAL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT

#define TRACE(fmt, ...) xlib::Log::Instance().Write(xlib::LOG_PRIORITY_TRACE, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define DBG(fmt, ...)   xlib::Log::Instance().Write(xlib::LOG_PRIORITY_DEBUG, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define INF(fmt, ...)   xlib::Log::Instance().Write(xlib::LOG_PRIORITY_INFO,  __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define ERR(fmt, ...)   xlib::Log::Instance().Write(xlib::LOG_PRIORITY_ERROR, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT
#define FATAL(fmt, ...) xlib::Log::Instance().Write(xlib::LOG_PRIORITY_FATAL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);  // NOLINT

typedef enum {
    LOG_PRIORITY_NULL = 0,
    LOG_PRIORITY_TRACE,
    LOG_PRIORITY_DEBUG,
    LOG_PRIORITY_INFO,
    LOG_PRIORITY_ERROR,
    LOG_PRIORITY_FATAL,
} LOG_PRIORITY;

class Log {
 protected:
    Log();

 public:
    ~Log();

    static Log& Instance() {
        static Log s_log_instance;
        return s_log_instance;
    }

    template <typename ...Args>
    void SWrite(LOG_PRIORITY pri, const char* file, uint32_t line, const char* function,
        const std::string& format, const Args&... args) {
        std::string temp(format);
        Format(&temp, args...);
        Write(pri, file, line, function, temp.c_str());
    }

    void Format(std::string* format) {}

    template <typename First, typename... Rest>
    void Format(std::string* format,  const First& first, const Rest&... rest) {
        size_t index = format->find_first_of("##");
        if (index == std::string::npos) {
            return;
        }
        std::ostringstream oss;
        oss << first;
        format->replace(index, 2, oss.str());
        Format(format, rest...);
    }

    void Write(LOG_PRIORITY pri, const char* file, uint32_t line, const char* function,
        const char* fmt, ...);

    void SetLogPriority(LOG_PRIORITY pri);
    int  SetLogFile(std::string file);
    void Close();

 private:
    LOG_PRIORITY m_log_priority;
    int          m_log_fd;
    std::string  m_log_file;
};

}  // namespace xlib

#endif  // XLIB_LOG_H_
