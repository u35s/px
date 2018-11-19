/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef XLIB_LOG_H_
#define XLIB_LOG_H_

#include "string"
#include "xlib/time.h"

namespace xlib {

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

#define TRACE(fmt, ...) xlib::Log::Instance().Write(xlib::LOG_PRIORITY_TRACE, fmt, ##__VA_ARGS__);
#define DBG(fmt, ...)   xlib::Log::Instance().Write(xlib::LOG_PRIORITY_DEBUG, fmt, ##__VA_ARGS__);
#define INF(fmt, ...)   xlib::Log::Instance().Write(xlib::LOG_PRIORITY_INFO, fmt, ##__VA_ARGS__);
#define ERR(fmt, ...)   xlib::Log::Instance().Write(xlib::LOG_PRIORITY_ERROR, fmt, ##__VA_ARGS__);
#define FATAL(fmt, ...) xlib::Log::Instance().Write(xlib::LOG_PRIORITY_FATAL, fmt, ##__VA_ARGS__);

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

    void Write(LOG_PRIORITY pri, const char* fmt, ...);
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
