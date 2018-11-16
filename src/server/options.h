/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef SERVER_OPTIONS_H_
#define SERVER_OPTIONS_H_

#include <memory.h>

struct Options {
    Options() {
        memset(this, 0, sizeof(Options));
    }
    char  forward_domain[255];
    char  forward_port[5];
    bool  forward;
    uint32_t listen_port;

    bool  daemon;
    char  log_file[255];
};

#endif  // SERVER_OPTIONS_H_
