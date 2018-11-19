/*
 * Copyright [2018] <Copyright u35s>
 */

#include "xlib/log.h"
#include "xlib/daemon.h"
#include "xlib/platform.h"

#include "server/ps.h"
#include "server/options.h"

int main(int argc, char **argv) {
    Options options;
    options.Init(argc, argv);

    if (options.daemon == true) {
        xlib::Daemon();
    }
    if (options.log_file.empty() == false) {
        xlib::Log::Instance().SetLogFile(options.log_file);
    }
    if (options.log_level != 0) {
        xlib::Log::Instance().SetLogPriority(xlib::LOG_PRIORITY(options.log_level));
    }
    INF("log level %d,local port %d, host %s:%s",
        options.log_level, options.listen_port,
        options.forward_domain.c_str(), options.forward_port.c_str());

    ProxyServer::Instance().Init(&options);
    ProxyServer::Instance().Serve();;
    return 0;
}
