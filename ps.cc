#include <sys/socket.h>
#include <cstdio>
#include <iostream>
#include "spdlog/spdlog.h"
#include "ps.h"
#include "pc.h"

ProxyServer::ProxyServer(int port){
	serverfd_ = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverfd_ == -1) {
	    spdlog::get("console")->info("accept error {:s},code:{:d}",strerror(errno),serverfd_);
		return;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
    
	if (int err = ::bind(serverfd_, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
	    spdlog::get("console")->info("accept error {:s},code:{:d}",strerror(errno),err);
		return;
	}
    fcntl(serverfd_, F_SETFL, fcntl(serverfd_, F_GETFL, 0) | O_NONBLOCK); 
	listen(serverfd_, 5);
	
    spdlog::get("console")->info("listening on port {:d}",port);

    io_.set<ProxyServer, &ProxyServer::Accept>(this);
	io_.start(serverfd_, ev::READ);

	sio_.set<&ProxyServer::SignalCallback>();
	sio_.start(SIGINT);
}

ProxyServer::~ProxyServer(){
   io_.stop();
   sio_.stop();
   close(serverfd_);
}

void ProxyServer::Accept(ev::io& watcher, int revents){
    if (EV_ERROR & revents) {
        std::perror("got invalid event");
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(watcher.fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_fd == -1) {
	    spdlog::get("console")->info("accept error {:s},code:{:d}",strerror(errno),client_fd);
		return;
	}
	new ProxyClient(client_fd,client_addr);
}

void ProxyServer::SignalCallback(ev::sig &signal, int revents) {
	spdlog::get("console")->info("accept signal {:d}",signal.signum);
	signal.loop.break_loop();
}

