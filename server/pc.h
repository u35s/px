/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef SERVER_PC_H_
#define SERVER_PC_H_

#include <list>
#include <sstream>
#include <string>
#include "xlib/net_util.h"
#include "xlib/xlib.h"

class ProxyClient{
 public:
    ProxyClient(uint64_t handle, xlib::NetIO* netio);
    ~ProxyClient();
    int Update(bool read, uint64_t handle);
    uint64_t GetPeerHandle();

 private:
    int Write();
    int Read();
    int PeerWrite();
    int PeerRead();
    int ParseRequest();

    std::list<xlib::Buffer*>     client_write_queue_;
    std::list<xlib::Buffer*>     remote_write_queue_;
    std::string err_;
    std::stringbuf first_buf_;
    std::stringbuf parsed_buf_;

    std::string m_remote_host;

    uint64_t m_handle;
    uint64_t m_peer_handle;

    int m_recv_data_len;
    int m_send_data_len;

    bool first_line_read_;
    bool parsed_;

    xlib::NetIO* m_netio;
};

#endif  // SERVER_PC_H_
