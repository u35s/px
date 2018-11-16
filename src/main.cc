/*
 * Copyright [2018] <Copyright u35s>
 */

#include "xlib/log.h"
#include "xlib/daemon.h"

#include "server/ps.h"
#include "server/options.h"

int main(int argc, char **argv) {
    Options options;
    options.Init(argc, argv);

    if (options.daemon) {
        xlib::Daemon();
    }
    if (options.log_file[0] != '\0') {
        xlib::Log::Instance().SetLogFile(options.log_file);
    }
    if (options.log_level != 0) {
        xlib::Log::Instance().SetLogPriority(xlib::LOG_PRIORITY(options.log_level));
    }
    INF("log level %d,local port %d, host %s:%s",
        options.log_level, options.listen_port, options.forward_domain, options.forward_port);

    ProxyServer::Instance().Init(&options);
    ProxyServer::Instance().Serve();;
    return 0;
}
