/*
 * Copyright [2018] <Copyright u35s>
 */

#include <stdio.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "server/options.h"
#include "xlib/xlib.h"

void Options::Init(int argc, char **argv) {
    int opt  = 0;
    while ( -1 != (opt = getopt(argc, argv, "dl:p:h:f:")) ) {
        switch (opt) {
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
                    snprintf(forward_domain, sizeof(forward_domain), "%s", vec[0].c_str());
                    snprintf(forward_port, sizeof(forward_port), "%s", vec[1].c_str());
                }
                break;
            }
            case 'f':
                snprintf(log_file, sizeof(log_file), "%s", optarg);
                break;
        }
    }
}
