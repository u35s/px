/*
 * Copyright [2018] <Copyright u35s>
 */

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "server/options.h"
#include "xlib/string.h"
#include "xlib/conv.h"

Options::Options() {
    daemon          = false;
    listen_port     = 1080;

    forward         = false;
    forward_port    = "1079";
    forward_domain  = "127.0.0.1";

    log_level       = 0;
    log_file        = "";
    auth            = "";
}

void Options::Init(int argc, char **argv) {
    int opt  = 0;
    while ( -1 != (opt = getopt(argc, argv, "dl:p:h:f:a:")) ) {
        switch (opt) {
            case 'a':
                auth = optarg;
                break;
            case 'd':
                daemon = true;
                break;
            case 'l':
                log_level = xlib::Atoi(optarg);
                break;
            case 'p':
                listen_port = xlib::Atoi(optarg);
                break;
            case 'h': {
                std::string host(optarg);
                std::vector<std::string> vec;
                xlib::Split(host, ":", &vec);
                if (vec.size() == 2) {
                    forward = true;
                    forward_domain = vec[0];
                    forward_port = vec[1];
                }
                break;
            }
            case 'f':
                log_file = optarg;
                break;
        }
    }
}
