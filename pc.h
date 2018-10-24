#ifndef PX_PC_H_
#define PX_PC_H_

#include <list>
#include <sstream>
#include "xlib/net_util.h"
#include "xlib/xlib.h"
#include "ps.h"

class ProxyClient{
public:
    ProxyClient(uint64_t handle, xlib::NetIO* netio);

	~ProxyClient();

   void Update(bool,uint64_t);
private:


   bool Write();
   bool Read();

   bool PeerWrite();
   bool PeerRead();

   void ParseRequest();

    std::list<xlib::Buffer*>     client_write_queue_;
    std::list<xlib::Buffer*>     remote_write_queue_;
	std::string err_;

	std::stringbuf first_buf_;
	std::stringbuf parsed_buf_;

    uint64_t m_handle;
    uint64_t m_peer_handle;
	bool first_line_read_;
	bool parsed_;
    xlib::NetIO* m_netio;
    //ProxyServer* m_ps;
};

#endif // end PX_PC_H_
