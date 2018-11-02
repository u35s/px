/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef XLIB_TIME_H_
#define XLIB_TIME_H_

#include <string>

namespace xlib {

class Time {
 public:
    Time();
    explicit Time(uint64_t second);
    std::string String();
    uint64_t Unix();
    ~Time();

    static Time Now();
    static uint64_t Micro();
 private:
    uint64_t m_second;
};

}  // namespace xlib

#endif  // XLIB_TIME_H_
