#ifndef PX_PC_H_
#define PX_PC_H_

#include <netinet/in.h> //sockaddr_in
#include <list>
#include <sstream>
#include "ev++.h"
#include "xlib.h"

class ProxyClient{
public:
    ProxyClient(const int fd,const struct sockaddr_in addr);
	~ProxyClient();
private:

   void ClientCallback(ev::io &watcher, int revents);
   bool ClientWriteCallback(ev::io &watcher);
   bool ClientReadCallback(ev::io &watcher);

   void RemoteCallback(ev::io &watcher, int revents);
   bool RemoteWriteCallback(ev::io &watcher);
   bool RemoteReadCallback(ev::io &watcher);

   void ParseRequest(ev::io &watcher, int revents);

    //ev::default_loop loop_;
    ev::io client_io_;
    ev::io remote_io_;
	int clientfd_;
	int remotefd_;
	struct sockaddr_in client_addr_;
	struct sockaddr_in remote_addr_;
    std::list<xlib::Buffer*>     client_write_queue_;
    std::list<xlib::Buffer*>     remote_write_queue_;
	std::string err_;

	bool parsed_;
	bool first_line_read_;
	std::stringbuf first_buf_;
	std::stringbuf parsed_buf_;
};

#endif // end PX_PC_H_
