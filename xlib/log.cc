/*
 * Copyright [2018] <Copyright u35s>
 */

#include <stdarg.h>
#include "xlib/log.h"

namespace xlib {

#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))

static const char*  g_priority_str[] = { "TRACE", "DEBUG", "INFO", "ERROR", "FATAL" };

Log::Log() {
    m_log_priority = LOG_PRIORITY_INFO;
}

Log::~Log() {
}

void Log::SetLogPriority(LOG_PRIORITY pri) {
    m_log_priority = pri;
}

void Log::Write(LOG_PRIORITY pri, const char* fmt, ...) {
    if (pri < m_log_priority) {
        return;
    }
    static char buff[4096] = {0};
    int pre_len = 0;
    pre_len = snprintf(buff, ARRAYSIZE(buff), "[%s][%s] ",
        xlib::Time::Now().String().c_str(), g_priority_str[pri]);
    if (pre_len < 0) {
        pre_len = 0;
    }
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buff + pre_len, ARRAYSIZE(buff) - pre_len, fmt, ap);
    va_end(ap);
    if (len < 0) {
        len = 0;
    }
    uint32_t tail = len + pre_len;
    if (tail > (ARRAYSIZE(buff) - 2)) {
        tail = ARRAYSIZE(buff) - 2;
    }
    buff[tail++] = '\n';
    buff[tail] = '\0';
    fprintf(stdout, "%s", buff);
}

}  // namespace xlib
