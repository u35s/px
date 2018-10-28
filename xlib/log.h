/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef XLIB_LOG_H_
#define XLIB_LOG_H_

#include "xlib/time.h"

namespace xlib {

#define DBG(fmt, ...) printf("[%s][DBG] " fmt "\n", xlib::Time::Now().String().c_str(), ##__VA_ARGS__);
// #define DBG(fmt, ...)
#define INF(fmt, ...) printf("[%s][INF] " fmt "\n", xlib::Time::Now().String().c_str(), ##__VA_ARGS__);
#define ERR(fmt, ...) printf("[%s][ERR] " fmt "\n", xlib::Time::Now().String().c_str(), ##__VA_ARGS__);

}  // namespace xlib

#endif  // XLIB_LOG_H_
