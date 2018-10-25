#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <cstdio>
#include <iostream>
#include "xlib/net_util.h"
#include "xlib/log.h"
#include "ps.h"

ProxyServer::ProxyServer()
    : m_netio(new xlib::NetIO()), m_port(0), m_epoll(new xlib::Epoll()) {
}

ProxyServer::~ProxyServer() {
    delete m_epoll;
    delete m_netio;
}

void ProxyServer::Init(uint32_t port) {
    m_port = port;
    m_epoll->Init(1024);
    m_netio->Init(m_epoll);
    m_netio->Listen("tcp://0.0.0.0", m_port);
}

void ProxyServer::NewClient(uint64_t handle) {
    std::shared_ptr<ProxyClient> client(new ProxyClient(handle, m_netio));
    client->Update(true, handle);
    m_client_map[handle] = client;
}

void ProxyServer::AddPeerClient(uint64_t handle, uint64_t peer_handle) {
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient>>::iterator it =
        m_client_map.find(handle);
    if (it == m_client_map.end()) {
        return;
    }
    m_client_map[peer_handle] = it->second;
}

void ProxyServer::RemoveClient(uint64_t handle) {
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient>>::iterator it =
        m_client_map.find(handle);
    if (it == m_client_map.end()) {
        return;
    }
    uint64_t peer_handle = it->second->GetPeerHandle();
    if (peer_handle != 0) {
        INF("remove peer client %lu", peer_handle);
        m_client_map.erase(peer_handle);
    }
    INF("remove client %lu", handle);
    m_client_map.erase(handle);
}

void ProxyServer::SendData(uint64_t handle) {
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient>>::iterator it =
        m_client_map.find(handle);
    if (it == m_client_map.end()) {
        return;
    }
    if (-1 == it->second->Update(false, handle)) {
        RemoveClient(handle);
    }
}

void ProxyServer::RecvData(uint64_t handle) {
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient>>::iterator it =
        m_client_map.find(handle);
    if (it == m_client_map.end()) {
        return;
    }
    if (-1 == it->second->Update(true, handle)) {
        RemoveClient(handle);
    }
}

void ProxyServer::Update() {
    int32_t ret = m_epoll->Wait(-1);
    if (ret <= 0) {
       return;
    }

    uint32_t events  = 0;
    uint64_t netaddr = 0;
    ret = m_epoll->GetEvent(&events, &netaddr);
    if (ret != 0) {
        return;
    }
    if (events & EPOLLERR) {
        RemoveClient(netaddr);
    }
    if (events & EPOLLOUT) {
        SendData(netaddr);
    }
    if (events & EPOLLIN) {
        const xlib::SocketInfo* socket_info = m_netio->GetSocketInfo(netaddr);
        if (socket_info->_state & xlib::TCP_PROTOCOL) {
            if (socket_info->_state & xlib::LISTEN_ADDR) {
                uint64_t peer_handle = m_netio->Accept(netaddr);
                if (peer_handle != xlib::INVAILD_NETADDR) {
                    NewClient(peer_handle);
                }
            } else {
                RecvData(netaddr);
            }
        }
    }
}

void ProxyServer::Serve() {
    do {
        Update();
    } while (true);
}
