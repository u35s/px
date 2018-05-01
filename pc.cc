#include <arpa/inet.h> //inet_ntoa
#include <boost/format.hpp>
#include "spdlog/spdlog.h"
#include "pc.h"
#include "xlib.h"

ProxyClient::ProxyClient(const int& fd,const struct sockaddr_in& addr):
	clientfd_(fd),client_addr_(addr){
		spdlog::get("console")->info("new client form {:s}:{:d},fd {:d}",
				inet_ntoa(client_addr_.sin_addr),ntohl(client_addr_.sin_port),fd);

		remote_addr_.sin_family = AF_INET;

		fcntl(clientfd_, F_SETFL, fcntl(clientfd_, F_GETFL, 0) | O_NONBLOCK); 
		client_io_.set<ProxyClient, &ProxyClient::ParseRequest>(this);
		client_io_.start(clientfd_, ev::READ);
	};

ProxyClient::~ProxyClient(){
	spdlog::get("console")->info("client {:s}:{:d} closed,err:{:s}",
			inet_ntoa(client_addr_.sin_addr),client_addr_.sin_port,err_);

	xlib::print_stack();
	client_io_.stop();
	close(clientfd_);
	if (parsed_) {
		remote_io_.stop();
		close(remotefd_);
	}
}

void ProxyClient::ParseRequest(ev::io &watcher, int revents){
	char buffer[1];
	char last;
	std::stringbuf* buf;
	while (true) {
		int nread = recv(clientfd_, buffer, 1, 0);
		if (nread ==0 ){
			delete this;
			return;
		} else if (nread == -1) {
			if (errno==EAGAIN) {
				return;
			} else {
				spdlog::get("console")->info("parse requet error {:s},code:{:d}",strerror(errno),nread);
				delete this;
				return;
			}
		}
		buf = &first_buf_;
		if (*buffer	== '\r') {
			continue;
		} else if (*buffer == '\n') {
			if (!first_line_read_) {
				first_line_read_ = true;
				continue;
			} else if (last == '\n') {
				break;
			}
		}	
		last = *buffer;
		if (first_line_read_) {
			buf = &parsed_buf_;
		}
		buf->sputn(buffer,1);	
	}
	std::vector<std::string> vec;
	xlib::split(first_buf_.str()," ",vec);
	if (vec.size() < 2) {
		err_ = (boost::format("%s:%s") %"read hostname error" %first_buf_.str()).str();  
		delete this;	
		return;
	}
	std::string host,port,proto;
	if (vec[0] == "CONNECT"){
		std::vector<std::string> two;
		xlib::split(vec[1],":",two);
		if (two.size() != 2) {
			err_ = (boost::format("https,%s:%s") %"extract host,port error" %vec[1]).str();  
			delete this;
			return;	
		}
		host = two[0];
		port = two[1];
		proto = "https";
	} else {
		std::vector<std::string> third;
		xlib::split(vec[1],"/",third);
		if (third.size() < 3) {
			err_ = (boost::format("http,%s:%s") %"extract host,port error" %vec[1]).str();  
			delete this;
			return;	
		}
		std::vector<std::string> ot;
		xlib::split(third[1],":",ot);
		if (ot.size() == 1) {
			host = ot[0];
			port = "80";
		} else if (ot.size() == 2) {
			host = ot[0];
			port = ot[1];
		} else {
			err_ = (boost::format("http,%s:%s") %"extract host,port error" %vec[1]).str();  
			delete this;
			return;	
		}
		proto = "http";
	}

	extern char* conf_domain;
	extern char* conf_port;
	extern bool conf_forward;

	char ip[16]; 
	std::string used_host = host,used_port = port;
	if (conf_forward) {
		used_host = std::string(conf_domain);
		used_port = std::string(conf_port);
	}
	xlib::get_ip_by_domain(used_host.c_str(),ip);
	remote_addr_.sin_addr.s_addr = inet_addr(ip);
	remote_addr_.sin_port = htons(std::stoi(used_port));

	spdlog::get("console")->info("new request {:s}:{:s} over {:s}:{:s},ip:{:s}:{:d},{},{:s}",
			host,port,used_host,used_port,ip,ntohs(remote_addr_.sin_port),proto=="https",parsed_buf_.str());

	char bt[1] = {'\n'};
	first_buf_.sputn(bt,1);	
	parsed_buf_.sputn(bt,1);
	if ( proto == "https" ) {
		if (conf_forward) {
			remote_write_queue_.push_back(new xlib::Buffer(first_buf_.str().c_str(),first_buf_.str().size()));
			remote_write_queue_.push_back(new xlib::Buffer(parsed_buf_.str().c_str(),parsed_buf_.str().size()));
		} else {
			std::string str = "HTTP/1.1 200 Connection established\r\n\r\n";
			client_write_queue_.push_back(new xlib::Buffer(str.c_str(),str.size()));
		}
	} else {
		remote_write_queue_.push_back(new xlib::Buffer(first_buf_.str().c_str(),first_buf_.str().size()));
		remote_write_queue_.push_back(new xlib::Buffer(parsed_buf_.str().c_str(),parsed_buf_.str().size()));
	}
	remotefd_ = xlib::connect_to(&remote_addr_);

	fcntl(remotefd_, F_SETFL, fcntl(remotefd_, F_GETFL, 0) | O_NONBLOCK); 
	remote_io_.set<ProxyClient, &ProxyClient::RemoteCallback>(this);
	remote_io_.start(remotefd_, ev::WRITE|ev::READ);

	client_io_.set<ProxyClient, &ProxyClient::ClientCallback>(this);
	client_io_.set(ev::READ|ev::WRITE);
}

void ProxyClient::ClientCallback(ev::io &watcher, int revents) {
	if (EV_ERROR & revents) {
		spdlog::get("console")->info("client callback got invalid event {:s}",strerror(errno));
		delete this;
		return;
	}

	if (revents & EV_READ) 
		ClientReadCallback(watcher);

	if (revents & EV_WRITE) 
		ClientWriteCallback(watcher);

	if (client_write_queue_.empty()) {
		client_io_.set(ev::READ);
	} else {
		client_io_.set(ev::READ|ev::WRITE);
	}
}

void ProxyClient::ClientReadCallback(ev::io &watcher) {
	char buffer[1024];
	memset(buffer,0,1024);
	int nread = recv(watcher.fd, buffer, sizeof(buffer), 0);

	if (nread == 0) {
		delete this;
		return;
	} else if (nread == -1) {
		if (errno==EAGAIN) {
			return;
		} else {
			spdlog::get("console")->info("client read error {:s},code:{:d}",strerror(errno),nread);
			delete this;
			return;
		}
	}

	//spdlog::get("console")->info("recive from client {:s}:{:d},length:{:d}",
	//		inet_ntoa(remote_addr_.sin_addr),remote_addr_.sin_port,nread);
	remote_write_queue_.push_back(new xlib::Buffer(buffer,nread));
	remote_io_.set(ev::READ|ev::WRITE);
}

void ProxyClient::ClientWriteCallback(ev::io &watcher) {
	if (client_write_queue_.empty()) {
		client_io_.set(ev::READ);
		return;
	}

	xlib::Buffer* buffer = client_write_queue_.front();
	int written = write(watcher.fd, buffer->Bytes(), buffer->Length());
	if (written == -1) {
		if (errno==EAGAIN) {
			return;
		} else {
			spdlog::get("console")->info("client write error {:s},code:{:d}",strerror(errno),written);
			delete this;
			return;
		}
	}

	//char send[1024];
	//memset(send,0,1024);
	//memcpy(send,buffer->Bytes(),written);
	//spdlog::get("console")->info("remote {:s}:{:d} send to client {:s}:{:d},length:{:d}",
	//		inet_ntoa(remote_addr_.sin_addr),remote_addr_.sin_port,
	//		inet_ntoa(client_addr_.sin_addr),client_addr_.sin_port,written);

	buffer->Add(written);
	if (buffer->Length() == 0) {
		client_write_queue_.pop_front();
		delete buffer;
	}
}

void ProxyClient::RemoteCallback(ev::io &watcher, int revents) {
	if (EV_ERROR & revents) {
		delete this;
		std::perror("remote callback got invalid event");
		return;
	}

	if (revents & EV_READ) 
		RemoteReadCallback(watcher);

	if (revents & EV_WRITE) 
		RemoteWriteCallback(watcher);

	if (remote_write_queue_.empty()) {
		remote_io_.set(ev::READ);
	} else {
		remote_io_.set(ev::READ|ev::WRITE);
	}
}

void ProxyClient::RemoteReadCallback(ev::io &watcher) {
	char buffer[1024];
	memset(buffer,0,1024);

	int nread = recv(watcher.fd, buffer, sizeof(buffer)-1, 0);

	if (nread == 0) {
        // connection closed
		delete this;
	} else if ( nread== -1) {
		if (errno==EAGAIN) {
			return;
		}  else {
			spdlog::get("console")->info("remote read error {:s},code:{:d}",
					strerror(errno),nread);
			delete this;
			return;
		}

	}

	//spdlog::get("console")->info("recive from remote {:s}:{:d},length:{:d}",
	//		inet_ntoa(remote_addr_.sin_addr),remote_addr_.sin_port,nread);

	client_write_queue_.push_back(new xlib::Buffer(buffer,nread));
	client_io_.set(ev::READ|ev::WRITE);
}

void ProxyClient::RemoteWriteCallback(ev::io &watcher) {
	if (remote_write_queue_.empty()) {
		remote_io_.set(ev::READ);
		return;
	}

	xlib::Buffer* buffer = remote_write_queue_.front();

	int written = write(watcher.fd, buffer->Bytes(), buffer->Length());
	if (written == -1) {
		if (errno == EAGAIN) {
			return;
		} else {
			spdlog::get("console")->info("remote write error {:s},code:{:d}",
					strerror(errno),written);
			delete this;
			return;
		}
	}

	//char send[1024];
	//memset(send,0,1024);
	//memcpy(send,buffer->Bytes(),written);
	//spdlog::get("console")->info("client {:s}:{:d} send to remote {:s}:{:d},length:{:d}",
	//		inet_ntoa(client_addr_.sin_addr),client_addr_.sin_port,
	//		inet_ntoa(remote_addr_.sin_addr),remote_addr_.sin_port,written);

	buffer->Add(written);
	if (buffer->Length() == 0) {
		remote_write_queue_.pop_front();
		delete buffer;
	}
}
