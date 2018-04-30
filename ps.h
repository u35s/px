#ifndef PX_PS_H_
#define PX_PS_H_

#include "ev++.h"

class ProxyServer{
public:
    ProxyServer(int port);
	~ProxyServer();

	void Accept(ev::io& watcher, int revents);
	static void SignalCallback(ev::sig &signal, int revents);
private:
    ev::io io_;
    ev::sig sio_;
    int serverfd_;
};

#endif // end PX_PS_H_
