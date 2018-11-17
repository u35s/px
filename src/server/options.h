/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef SERVER_OPTIONS_H_
#define SERVER_OPTIONS_H_

#include <memory.h>
#include <stdint.h>

struct Options {
    Options() {
        memset(this, 0, sizeof(Options));
    }
    void Init(int argc, char **argv);

    char  forward_domain[255];
    char  forward_port[5];
    bool  forward;
    uint32_t listen_port;

    bool  daemon;
    char  log_file[255];

    uint32_t log_level;
};

#endif  // SERVER_OPTIONS_H_
