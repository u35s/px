/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef SERVER_PC_H_
#define SERVER_PC_H_

#include <list>
#include <sstream>
#include <string>
#include "xlib/net_util.h"
#include "xlib/buffer.h"

#define MAX_QUEUE_SIZE 10

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
    bool VerifyAuthorization();

    xlib::Buffer* m_buffer;
    xlib::Buffer* m_peer_buffer;

    std::string m_remote_host;

    bool m_reading_wait;
    bool m_peer_reading_wait;

    uint64_t m_handle;
    uint64_t m_peer_handle;

    int m_recv_data_len;
    int m_send_data_len;

    bool m_first_line_read;
    bool m_parsed;
    std::string m_first_line_buf;
    std::string m_parse_buf;
    std::string m_authorization;

    xlib::NetIO* m_netio;
};

#endif  // SERVER_PC_H_
