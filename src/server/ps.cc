/*
 * Copyright [2018] <Copyright u35s>
 */

#include <stdlib.h>
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

void on_stop(int32_t signal) {
    g_app_events._stop = signal;
}

void on_log_level(int signal) {
    if (signal == SIGUSR1) {
        g_app_events._log_level = xlib::LOG_PRIORITY_INFO;
    } else if (signal == SIGUSR2) {
        g_app_events._log_level = xlib::LOG_PRIORITY_TRACE;
    }
    xlib::Log::Instance().SetLogPriority(g_app_events._log_level);
    INF("set log level %d", g_app_events._log_level);
}

ProxyServer::ProxyServer()
    : m_netio(NULL), m_options(NULL), m_poll(NULL) {
}

ProxyServer::~ProxyServer() {
    // delete m_poll;
    // delete m_netio;
}

// T const * | const T *  常量指针 不能通过指针变量直接更改指针指向的值
// T * const              指针常量 指针变量不允许修改
// T const * const

const Options *const ProxyServer::GetOptions() {
    return m_options;
}

void ProxyServer::Init(Options* options) {
    m_options  = options;
    m_netio = new xlib::NetIO();
#if defined(__linux__)
    m_poll  = new xlib::Epoll();
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
    m_poll  = new xlib::Kqueue();
#endif
    m_poll->Init(1024);
    m_netio->Init(m_poll);
    auto netaddr = m_netio->Listen("tcp://0.0.0.0", m_options->listen_port);
    if (xlib::INVAILD_NETADDR == netaddr) {
        exit(0);
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, on_stop);
    signal(SIGINT, on_stop);
    signal(SIGUSR1, on_log_level);
    signal(SIGUSR2, on_log_level);
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
    int32_t num = m_poll->Wait(0);
    if (num <= 0) {
       return 0;
    }

    while (true) {
       if (ProcessMessage() <= 0) {
           break;
       }
    }
    if (num > 0) {
        TRACE("process message %d", num);
    }
    return num;
}

uint64_t ProxyServer::ProcessMessage() {
    uint32_t events  = 0;
    uint64_t netaddr = 0;
    int32_t ret = m_poll->GetEvent(&events, &netaddr);
    if (ret != 0) {
        return 0;
    }
    TRACE("process message, events:%u, netaddr:%lu, ret:%d", events, netaddr, ret);
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
        uint64_t elapsed = end - start;
        if (elapsed < 1000) {
            usleep(1000 - elapsed);
        }
        if (elapsed > 300) {
            INF("update process %lu msg, run %lu microseond", num, elapsed);
        }
    } while (true);
}
