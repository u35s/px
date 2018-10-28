/*
 * Copyright [2018] <Copyright u35s>
 */

#include <sys/time.h>
#include <time.h>
#include <string>

#include "xlib/time.h"

namespace xlib {

Time::Time() : m_second(0) {}

Time::Time(uint64_t second) : m_second(second) {}

Time::~Time() {}

Time Time::Now() {
    time_t sec = time(NULL);
    return Time(uint64_t(sec));
}

std::string Time::String() {
    time_t now = m_second;
    struct tm tm_now;
    struct tm* p_tm_now;

    p_tm_now = localtime_r(&now, &tm_now);

    char buff[256] = {0};
    snprintf(buff, sizeof(buff), "%04d-%02d-%02d% 02d:%02d:%02d",
        1900 + p_tm_now->tm_year,
        p_tm_now->tm_mon + 1,
        p_tm_now->tm_mday,
        p_tm_now->tm_hour,
        p_tm_now->tm_min,
        p_tm_now->tm_sec);

    return std::string(buff);
}

uint64_t Time::Micro() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t micro = tv.tv_sec * 1000000 + tv.tv_usec;
    return micro;
}

}  // namespace xlib
