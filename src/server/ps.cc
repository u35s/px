/*
 * Copyright [2018] <Copyright u35s>
 */

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>

#include "xlib/net_util.h"
#include "xlib/net_poll.h"
#include "xlib/log.h"
#include "xlib/time.h"
#include "server/ps.h"

#if defined(__linux__)
    #include "xlib/net_epoll.h"
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
    #include "xlib/net_kqueue.h"
#endif

static struct {
    int32_t _stop;
} g_app_events = { 0 };

void on_stop(int32_t signal) {
    g_app_events._stop = signal;
}

ProxyServer::ProxyServer()
    : m_netio(NULL), m_port(0), m_poll(NULL) {
}

ProxyServer::~ProxyServer() {
    delete m_poll;
    delete m_netio;
}

void ProxyServer::Init(uint32_t port) {
    m_port  = port;
    m_netio = new xlib::NetIO();
#if defined(__linux__)
    m_poll  = new xlib::Epoll();
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
    m_poll  = new xlib::Kqueue();
#endif
    m_poll->Init(1024);
    m_netio->Init(m_poll);
    m_netio->Listen("tcp://0.0.0.0", m_port);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, on_stop);
    signal(SIGINT, on_stop);
    setbuf(stdout, NULL);
}

void ProxyServer::NewClient(uint64_t handle) {
    std::shared_ptr<ProxyClient> client(new ProxyClient(handle, m_netio));
    m_client_map[handle] = client;
    client->Update(true, handle);
}

void ProxyServer::AddPeerClient(uint64_t handle, uint64_t peer_handle) {
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient>>::iterator it =
        m_client_map.find(handle);
    if (it == m_client_map.end()) {
        DBG("%lu add peer client no handle, %lu", handle, peer_handle);
        return;
    }
    m_client_map[peer_handle] = it->second;
    DBG("handle %lu, add peer client %lu", handle, peer_handle);
}

void ProxyServer::RemoveClient(uint64_t handle) {
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient>>::iterator it =
        m_client_map.find(handle);
    if (it == m_client_map.end()) {
        return;
    }
    uint64_t peer_handle = it->second->GetPeerHandle();
    if (peer_handle != 0) {
        DBG("remove peer client %lu", peer_handle);
        m_client_map.erase(peer_handle);
    }
    DBG("remove client %lu", handle);
    m_client_map.erase(handle);
}

void ProxyServer::SendData(uint64_t handle) {
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient>>::iterator it =
        m_client_map.find(handle);
    if (it == m_client_map.end()) {
        DBG("send data no handle, %lu", handle);
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
        DBG("recv data no handle, %lu", handle);
        return;
    }
    if (-1 == it->second->Update(true, handle)) {
        RemoveClient(handle);
    }
}

uint64_t ProxyServer::Update() {
    uint64_t num = 0;
    for (uint32_t i = 0; i < 100; i++) {
       if (ProcessMessage() <= 0) {
           break;
       }
       num++;
    }
    if (num > 0) {
        // TRACE("process message %lu", num);
    }
    return num;
}

uint64_t ProxyServer::ProcessMessage() {
    int32_t ret = m_poll->Wait(0);
    if (ret <= 0) {
       return 0;
    }

    uint32_t events  = 0;
    uint64_t netaddr = 0;
    ret = m_poll->GetEvent(&events, &netaddr);
    TRACE("process message, events:%u, netaddr:%lu, ret:%d", events, netaddr, ret);
    if (ret != 0) {
        return 0;
    }
    if (events & POLLERR) {
        RemoveClient(netaddr);
    }
    if (events & POLLOUT) {
        SendData(netaddr);
    }
    if (events & POLLIN) {
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
    return 1;
}

uint64_t ProxyServer::Stop() {
    DBG("stoped");
    return 0;
}

void ProxyServer::Idle() {
    usleep(1000);
}

void ProxyServer::Serve() {
    uint64_t num = 0;
    do {
         if (g_app_events._stop) {
            if (Stop() == 0) {
                g_app_events._stop = 0;
                break;
            }
        }

        uint64_t start = xlib::Time::Micro();
        num = Update();
        uint64_t end = xlib::Time::Micro();

        if (num <= 0) {
            Idle();
        } else {
            TRACE("update process %lu msg, run %lu microseond", num, end-start);
        }
    } while (true);
}
