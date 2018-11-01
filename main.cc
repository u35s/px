/*
 * Copyright [2018] <Copyright u35s>
 */

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "xlib/xlib.h"
#include "xlib/log.h"

#include "server/ps.h"

char  conf_domain[255];
char  conf_port[5];
bool  conf_forward = false;

xlib::LOG_PRIORITY log_level = xlib::LOG_PRIORITY_INFO;

ProxyServer ps;

int main(int argc, char **argv) {
    int port = 1080;
    int opt  = 0;
    while ( -1 != (opt = getopt(argc, argv, "l:p:h:")) ) {
        switch (opt) {
            case 'l':
                log_level = xlib::LOG_PRIORITY(xlib::Atoi(optarg));
                break;
            case 'p':
                port = xlib::Atoi(optarg);
                break;
            case 'h':
                std::string host(optarg);
                std::vector<std::string> vec;
                xlib::Split(host, ":", &vec);
                if (vec.size() == 2) {
                    conf_forward = true;
                    snprintf(conf_domain, sizeof(conf_domain), "%s", vec[0].c_str());
                    snprintf(conf_port, sizeof(conf_port), "%s", vec[1].c_str());
                }
                break;
        }
    }
    xlib::Log::Instance().SetLogPriority(log_level);
    INF("log level %d,local port %d, host %s:%s",
        log_level, port, conf_domain, conf_port);
    ps.Init(port);
    ps.Serve();
    return 0;
}
