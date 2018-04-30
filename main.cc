#include "ev++.h"
#include "spdlog/spdlog.h"
#include "ps.h"
#include "xlib.h"

char* conf_domain;
char* conf_port;
bool conf_forward = false;

int main(int argc, char **argv){
	spdlog::stdout_logger_mt("console");

	int port = 1079;
	if (argc >= 2){
	    port = xlib::atoi(argv[1]);
		if (argc >= 4 ){
			conf_forward = true;	
			conf_domain = argv[2];
			conf_port = argv[3];
		}
	}
    ev::default_loop loop;
	ProxyServer px(port);
    loop.run(0);
    return 0;
}
