#ifndef PX_PS_H_
#define PX_PS_H_

#include <memory>
#include "xlib/net_util.h"
#include "pc.h"

class ProxyClient;
class ProxyServer{
public:
    ProxyServer();
	~ProxyServer();

	void Init(uint32_t port);
	void NewClient(uint64_t);
	void AddPeerClient(uint64_t,uint64_t);
	void RemoveClient(uint64_t);
	void SendData(uint64_t);
	void RecvData(uint64_t);
    void Serve();
    void Update();
    xlib::NetIO* m_netio;
private:
    uint32_t m_port;
    xlib::Epoll* m_epoll;
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient> > m_client_map;
};

#endif // end PX_PS_H_
