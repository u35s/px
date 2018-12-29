/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef SERVER_OPTIONS_H_
#define SERVER_OPTIONS_H_

#include <stdint.h>
#include <string>

struct Options {
    Options();

    void Init(int argc, char **argv);

    bool         daemon;
    uint32_t     listen_port;

    bool         forward;
    std::string  forward_port;
    std::string  forward_domain;

    uint32_t     log_level;
    std::string  log_file;
    std::string  auth;
};

#endif  // SERVER_OPTIONS_H_
