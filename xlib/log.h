#ifndef PX_XLIB_LOG_H_
#define PX_XLIB_LOG_H_

#define LOG(fmt, ...) printf(fmt,  ##__VA_ARGS__);
#define DBG(fmt, ...) printf(fmt,  ##__VA_ARGS__);printf("\n");
#define INF(fmt, ...) printf(fmt,  ##__VA_ARGS__);printf("\n");
#define ERR(fmt, ...) printf(fmt,  ##__VA_ARGS__);printf("\n");

#endif  // end PX_XLIB_LOG_H_
