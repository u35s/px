#include "xlib/xlib.h"
#include "xlib/log.h"

#include "ps.h"

char* conf_domain;
char* conf_port;
bool  conf_forward = false;

ProxyServer ps;

int main(int argc, char **argv) {
    int port = 1079;
    if (argc >= 2) {
        port = xlib::atoi(argv[1]);
        if (argc >= 4) {
            conf_forward = true;
            conf_domain = argv[2];
            conf_port = argv[3];
        }
    }
    ps.Init(port);
    ps.Serve();
    return 0;
}
