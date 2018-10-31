/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef SERVER_PS_H_
#define SERVER_PS_H_

#include <memory>
#include <unordered_map>
#include "xlib/net_util.h"
#include "server/pc.h"

class ProxyServer{
 public:
    ProxyServer();
    ~ProxyServer();

    void Init(uint32_t port);
    void NewClient(uint64_t handle);
    void AddPeerClient(uint64_t handle, uint64_t peer_handle);
    void RemoveClient(uint64_t handle);
    void SendData(uint64_t handle);
    void RecvData(uint64_t handle);
    void Serve();
    void Idle();

    uint64_t Update();
    uint64_t Stop();
    uint64_t ProcessMessage();

    xlib::NetIO* m_netio;
 private:
    uint32_t m_port;
    xlib::Poll* m_poll;
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient> > m_client_map;
};

#endif  // SERVER_PS_H_
