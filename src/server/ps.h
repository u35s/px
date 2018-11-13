/*
 * Copyright [2018] <Copyright u35s>
 */

#ifndef SERVER_PS_H_
#define SERVER_PS_H_

#include <memory>
#include <unordered_map>
#include "xlib/net_util.h"
#include "xlib/log.h"
#include "server/pc.h"
#include "server/options.h"

static struct {
    int32_t _stop;
    xlib::LOG_PRIORITY _log_level;
} g_app_events = { 0, xlib::LOG_PRIORITY_INFO };

class ProxyServer {
 protected:
    ProxyServer();

 public:
    static ProxyServer& Instance() {
        static ProxyServer s_ps_instance;
        return s_ps_instance;
    }
    ~ProxyServer();

    const Options *const GetOptions();
    void Init(Options* options);
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
    Options*  m_options;
    xlib::Poll* m_poll;
    std::unordered_map<uint64_t, std::shared_ptr<ProxyClient> > m_client_map;
};

#endif  // SERVER_PS_H_
