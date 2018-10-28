/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef XLIB_LOG_H_
#define XLIB_LOG_H_

#define LOG(fmt, ...) printf(fmt,  ##__VA_ARGS__);
// #define DBG(fmt, ...) printf(fmt,  ##__VA_ARGS__); printf("\n)
#define DBG(fmt, ...)
#define INF(fmt, ...) printf(fmt,  ##__VA_ARGS__); printf("\n")
#define ERR(fmt, ...) printf(fmt,  ##__VA_ARGS__); printf("\n")

#endif  // XLIB_LOG_H_
