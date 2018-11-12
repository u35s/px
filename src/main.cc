/*
 * Copyright [2018] <Copyright u35s>
 */

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "xlib/xlib.h"
#include "xlib/log.h"

#include "server/ps.h"
#include "server/options.h"

int main(int argc, char **argv) {
    Options options;
    int opt  = 0;
    while ( -1 != (opt = getopt(argc, argv, "l:p:h:")) ) {
        switch (opt) {
            case 'l':
                g_app_events._log_level = xlib::LOG_PRIORITY(xlib::Atoi(optarg));
                break;
            case 'p':
                options.listen_port = xlib::Atoi(optarg);
                break;
            case 'h':
                std::string host(optarg);
                std::vector<std::string> vec;
                xlib::Split(host, ":", &vec);
                if (vec.size() == 2) {
                    options.forward = true;
                    snprintf(options.forward_domain, sizeof(options.forward_domain), "%s", vec[0].c_str());
                    snprintf(options.forward_port, sizeof(options.forward_port), "%s", vec[1].c_str());
                }
                break;
        }
    }
    xlib::Log::Instance().SetLogPriority(g_app_events._log_level);
    INF("log level %d,local port %d, host %s:%s",
        g_app_events._log_level, options.listen_port, options.forward_domain, options.forward_port);

    ProxyServer::Instance().Init(&options);
    ProxyServer::Instance().Serve();;
    return 0;
}
