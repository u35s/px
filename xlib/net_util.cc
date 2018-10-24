#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include "net_util.h"
#include "log.h"

namespace xlib {

void SocketInfo::Reset() {
    _socket_fd = -1;
    _addr_info = UINT32_MAX;
    _ip = UINT32_MAX;
    _port = UINT16_MAX;
    _state = 0;
    ++_uin;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

Epoll::Epoll() : m_epoll_fd(-1), m_max_event(1000), m_event_num(0),
    m_events(NULL), m_bind_net_io(NULL) {
}

Epoll::~Epoll() {
    if (m_epoll_fd >= 0) {
        close(m_epoll_fd);
    }
    if (NULL != m_events) {
        m_event_num = 0;
        delete []m_events;
    }
}

int32_t Epoll::Init(uint32_t max_event) {
    m_last_error[0] = 0;
    m_max_event = static_cast<int32_t>(max_event);
    m_epoll_fd = epoll_create(m_max_event);
    m_events = new struct epoll_event[max_event];
    if (m_epoll_fd < 0) {
        ERR("epoll_create failed in %d", errno);
        return -1;
    }
    return 0;
}

int32_t Epoll::Wait(int32_t timeout) {
    m_event_num = epoll_wait(m_epoll_fd, m_events, m_max_event, timeout);
    return m_event_num;
}

int32_t Epoll::AddFd(int32_t fd, uint32_t events, uint64_t data) {
    struct epoll_event eve;
    eve.events = events;
    eve.data.u64 = data;
    return epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &eve);
}

int32_t Epoll::DelFd(int32_t fd) {
    return epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

int32_t Epoll::ModFd(int32_t fd, uint32_t events, uint64_t data) {
    struct epoll_event eve;
    eve.events = events;
    eve.data.u64 = data;
    return epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &eve);
}

int32_t Epoll::GetEvent(uint32_t *events, uint64_t *data) {
    if (m_event_num > 0) {
        --m_event_num;
        *events = m_events[m_event_num].events;
        *data = m_events[m_event_num].data.u64;
        if (NULL != m_bind_net_io) {
            m_bind_net_io->OnEvent(*data, *events);
        }
        return 0;
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

bool NetIO::NON_BLOCK = true;
bool NetIO::ADDR_REUSE = true;
bool NetIO::KEEP_ALIVE = true;
bool NetIO::USE_NAGLE = false;
bool NetIO::USE_LINGER = false;
int32_t NetIO::LINGER_TIME = 0;
int32_t NetIO::LISTEN_BACKLOG = 10240;
uint32_t NetIO::MAX_SOCKET_NUM = 1000000;
uint8_t NetIO::AUTO_RECONNECT = 3;

NetIO::NetIO() : m_used_id(0), m_sockets(NULL) {
}

NetIO::~NetIO() {
    CloseAll();
}

int32_t NetIO::Init(Epoll* epoll) {
    m_last_error[0] = 0;
    m_epoll = epoll;
    if (NULL != m_epoll) {
        m_epoll->m_bind_net_io = this;
    }
    m_sockets = new SocketInfo[MAX_SOCKET_NUM];
    memset(m_sockets, 0, MAX_SOCKET_NUM * sizeof(SocketInfo));
    return 0;
}

NetAddr NetIO::Listen(const std::string& ip, uint16_t port) {
    NetAddr net_addr = AllocNetAddr();
    if (net_addr == INVAILD_NETADDR) {
        ERR("open net addr max then %u", MAX_SOCKET_NUM);
        return net_addr;
    }

    do {
        SocketInfo *socket_info = &m_sockets[static_cast<uint32_t>(net_addr)];
        if (0 != InitSocketInfo(ip, port, socket_info)) {
            ERR("invalid address[%s:%u]", ip.c_str(), port);
            break;
        }
        socket_info->_state |= LISTEN_ADDR;
        if (RawListen(net_addr, socket_info) < 0) {
            ERR("open listen address[%s:%u] fail", ip.c_str(), port);
            break;
        }
        return net_addr;
    } while (false);

    FreeNetAddr(net_addr);
    return INVAILD_NETADDR;
}

NetAddr NetIO::Accept(NetAddr listen_addr) {
    SocketInfo* socket_info = RawGetSocketInfo(listen_addr);
    if (NULL == socket_info
        || 0 == (socket_info->_state & LISTEN_ADDR)
        || 0 == (socket_info->_state & TCP_PROTOCOL)) {
        ERR("accept an addr[%lu] not be listened", listen_addr);
        return INVAILD_NETADDR;
    }
    if (socket_info->_socket_fd < 0) {
        RawListen(listen_addr, socket_info);
        if (socket_info->_socket_fd < 0) {
            ERR("addr[%lu] redo listen failed", listen_addr);
            return INVAILD_NETADDR;
        }
    }

    struct sockaddr_in cli_addr;
    socklen_t addr_len = sizeof(cli_addr);
    int32_t new_socket = accept(socket_info->_socket_fd,
        reinterpret_cast<struct sockaddr*>(&cli_addr), &addr_len);
    if (new_socket < 0) {
        if (errno == EBADF || errno == ENOTSOCK) {
            ERR("unexpect errno[%d] when accept addr[%lu], close socket[%d]",
                errno, listen_addr, socket_info->_socket_fd);
            RawClose(socket_info);
        }
        INF("accept none from addr[%lu]", listen_addr);
        return INVAILD_NETADDR;
    }
    if (true == NetIO::NON_BLOCK) {
        int32_t flags_val = fcntl(new_socket, F_GETFL);
        if (-1 != flags_val) {
            if (fcntl(new_socket, F_SETFL, flags_val | O_NONBLOCK) == -1) {
                ERR("fcntl socket failed in %d", errno);
                close(new_socket);
                return INVAILD_NETADDR;
            }
        }
    }

    NetAddr net_addr = AllocNetAddr();
    if (INVAILD_NETADDR == net_addr) {
        ERR("open net addr max then %u", MAX_SOCKET_NUM);
        close(new_socket);
        return INVAILD_NETADDR;
    }
    SocketInfo *new_socket_info = &m_sockets[static_cast<uint32_t>(net_addr)];
    new_socket_info->_socket_fd = new_socket;
    new_socket_info->_addr_info = static_cast<uint32_t>(listen_addr);
    new_socket_info->_ip = cli_addr.sin_addr.s_addr;
    new_socket_info->_port = cli_addr.sin_port;
    new_socket_info->_state |= (TCP_PROTOCOL | ACCEPT_ADDR);
    if (NULL != m_epoll) {
        m_epoll->AddFd(new_socket, EPOLLIN | EPOLLERR, net_addr);
    }
    return net_addr;
}

NetAddr NetIO::ConnectPeer(const std::string& ip, uint16_t port) {
    NetAddr net_addr = AllocNetAddr();
    if (net_addr == INVAILD_NETADDR) {
        ERR("open net addr max then %u", MAX_SOCKET_NUM);
        return net_addr;
    }

    do {
        SocketInfo *socket_info = &m_sockets[static_cast<uint32_t>(net_addr)];
        if (0 != InitSocketInfo(ip, port, socket_info)) {
            ERR("invalid address[%s:%u]", ip.c_str(), port);
            break;
        }
        // socket_info->_state |= CONNECT_ADDR;
        if (RawConnect(net_addr, socket_info) < 0) {
            ERR("connect address[%s:%u] fail", ip.c_str(), port);
            break;
        }
        return net_addr;
    } while (false);

    FreeNetAddr(net_addr);
    return INVAILD_NETADDR;
}

int32_t NetIO::Send(NetAddr dst_addr, const char* data, uint32_t data_len) {
    SocketInfo* socket_info = RawGetSocketInfo(dst_addr);
    if (NULL == socket_info
        || 0 != (LISTEN_ADDR & socket_info->_state)) {
        ERR("send an invalid addr[%lu]", dst_addr);
        return -1;
    }
    if (socket_info->_socket_fd < 0) {
        if (socket_info->_state & CONNECT_ADDR) {
            RawConnect(dst_addr, socket_info);
        }
        if (socket_info->_socket_fd < 0) {
            ERR("cannot connect addr[%lu] for send", dst_addr);
            return -1;
        }
    }

    int32_t send_ret = 0;
    uint32_t send_cnt = 0;
    while (send_cnt < data_len) {
        send_ret = send(socket_info->_socket_fd, data + send_cnt, data_len - send_cnt, 0);
        if ((send_ret < 0 && errno != EINTR) || send_ret == 0) {
            break;
        }
        send_cnt += send_ret;
    }
    if (send_cnt == data_len) {
        return static_cast<int32_t>(send_cnt);
    }
    if (send_ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        ERR("send failed in %d, need close the socket[%d]", errno, socket_info->_socket_fd);
        RawClose(socket_info);
        return -1;
    }
    return static_cast<int32_t>(send_cnt);
}

int32_t NetIO::Recv(NetAddr dst_addr, char* buff, uint32_t buff_len) {
    SocketInfo *socket_info = RawGetSocketInfo(dst_addr);
    if (NULL == socket_info
        || 0 != (LISTEN_ADDR & socket_info->_state)) {
        ERR("recv an invalid addr[%lu]", dst_addr);
        return -1;
    }
    if (socket_info->_socket_fd < 0) {
        return 0;
    }

    int32_t recv_ret = 0;
    do {
        recv_ret = recv(socket_info->_socket_fd, buff, buff_len, 0);
    } while (recv_ret < 0 && errno == EINTR);

    if (recv_ret == 0 || (recv_ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        RawClose(socket_info);
        recv_ret = -1;
    } else if (recv_ret < 0) {
        recv_ret = 0;
    }
    return recv_ret;
}

int32_t NetIO::Close(NetAddr dst_addr) {
    SocketInfo* socket_info = RawGetSocketInfo(dst_addr);
    if (NULL == socket_info || 0 == socket_info->_state) {
        ERR("close an invalid addr[0x%lx]", dst_addr);
        return 0;
    }

    int32_t ret = RawClose(socket_info);
    FreeNetAddr(dst_addr);
    return ret;
}

int32_t NetIO::Reset(NetAddr dst_addr) {
    SocketInfo* socket_info = RawGetSocketInfo(dst_addr);
    if (NULL == socket_info) {
        ERR("close an invalid addr[0x%lx]", dst_addr);
        return -1;
    }

    if (socket_info->_state != (CONNECT_ADDR | TCP_PROTOCOL)) {
        ERR("cannt reset state = 0x%x", socket_info->_state);
        return -2;
    }

    RawClose(socket_info);
    return RawConnect(dst_addr, socket_info);
}

void NetIO::CloseAll() {
    for (NetAddr idx = 0 ; idx < m_used_id ; ++idx) {
        if (0 != m_sockets[idx]._state) {
            RawClose(&m_sockets[idx]);
        }
    }
    m_free_sockets.clear();
    m_used_id = 0;
}

const SocketInfo* NetIO::GetSocketInfo(NetAddr dst_addr) const {
    static SocketInfo INVALID_SOCKET_INFO = { 0 };
    uint32_t idx = static_cast<uint32_t>(dst_addr);
    if (idx >= MAX_SOCKET_NUM
        || m_sockets[idx]._uin != static_cast<uint8_t>(dst_addr >> 32)) {
        return &INVALID_SOCKET_INFO;
    }
    return &m_sockets[idx];
}

const SocketInfo* NetIO::GetLocalListenSocketInfo(NetAddr dst_addr) const {
    static SocketInfo INVALID_LISTEN_SOCKET_INFO = { 0 };
    uint32_t idx = static_cast<uint32_t>(dst_addr);
    if (idx >= MAX_SOCKET_NUM
        || m_sockets[idx]._uin != static_cast<uint8_t>(dst_addr >> 32)
        || m_sockets[idx]._addr_info >= MAX_SOCKET_NUM
        || 0 == (m_sockets[idx]._state & ACCEPT_ADDR)) {
        return &INVALID_LISTEN_SOCKET_INFO;
    }
    return &m_sockets[m_sockets[idx]._addr_info];
}

NetAddr NetIO::GetLocalListenAddr(NetAddr dst_addr) {
    uint32_t idx = static_cast<uint32_t>(dst_addr);
    if (idx >= MAX_SOCKET_NUM
        || m_sockets[idx]._uin != static_cast<uint8_t>(dst_addr >> 32)
        || m_sockets[idx]._addr_info >= MAX_SOCKET_NUM
        || 0 == (m_sockets[idx]._state & ACCEPT_ADDR)) {
        return INVAILD_NETADDR;
    }

    uint32_t addr_info = m_sockets[idx]._addr_info;
    return (static_cast<uint64_t>(m_sockets[addr_info]._uin) << 32) | addr_info;
}


NetAddr NetIO::AllocNetAddr() {
    NetAddr ret = INVAILD_NETADDR;
    if (0 != m_free_sockets.size()) {
        ret = m_free_sockets.front();
        m_free_sockets.pop_front();
    } else if (m_used_id < MAX_SOCKET_NUM) {
        ret = (m_used_id++);
        m_sockets[ret].Reset();
        ret |= (static_cast<uint64_t>(m_sockets[ret]._uin) << 32);
    }
    return ret;
}

void NetIO::FreeNetAddr(NetAddr net_addr) {
    uint32_t idx = static_cast<uint32_t>(net_addr);
    m_sockets[idx].Reset();
    net_addr = (static_cast<uint64_t>(m_sockets[idx]._uin) << 32) | idx;
    m_free_sockets.push_back(net_addr);
}

SocketInfo* NetIO::RawGetSocketInfo(NetAddr net_addr) {
    if (static_cast<uint32_t>(net_addr) >= MAX_SOCKET_NUM) {
        return NULL;
    }
    SocketInfo* sock_info = &m_sockets[static_cast<uint32_t>(net_addr)];
    if (sock_info->_uin != static_cast<uint8_t>(net_addr >> 32)) {
        return NULL;
    }
    return sock_info;
}

int32_t NetIO::InitSocketInfo(const std::string& ip, uint16_t port, SocketInfo* socket_info) {
    if (0 == ip.compare(0, 6, "tcp://")) {
        socket_info->_state |= TCP_PROTOCOL;
        socket_info->_ip = inet_addr(ip.c_str() + 6);
    } else if (0 == ip.compare(0, 6, "udp://")) {
        socket_info->_state |= UDP_PROTOCOL;
        socket_info->_ip = inet_addr(ip.c_str() + 6);
    } else {
        DBG("ip[%s] default as tcp protocol", ip.c_str());
        socket_info->_state |= TCP_PROTOCOL;
        socket_info->_ip = inet_addr(ip.c_str());
    }

    socket_info->_port = htons(port);
    if (socket_info->_ip == 0xFFFFFFFFU) {
        ERR("unknown ip[%s] address, it should be ipv4 xx.xx.xx.xx", ip.c_str());
        return -1;
    }
    return 0;
}

int32_t NetIO::RawListen(NetAddr net_addr, SocketInfo* socket_info) {
    int32_t s_fd = socket(AF_INET,
        ((socket_info->_state & TCP_PROTOCOL) ? SOCK_STREAM : SOCK_DGRAM), 0);
    if (s_fd < 0) {
        ERR("socket failed in %d", errno);
        return -1;
    }

    int32_t ret = fcntl(s_fd, F_GETFL);
    ret = ((ret < 0 || false == NetIO::NON_BLOCK)
        ? ret : (fcntl(s_fd, F_SETFL, ret | O_NONBLOCK)));
    if (ret < 0) {
        ERR("socket set opt failed in %d", errno);
        close(s_fd);
        return -1;
    }

    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = socket_info->_ip;
    addr.sin_port = socket_info->_port;
    ret = bind(s_fd, reinterpret_cast<struct sockaddr*>(&addr), addrlen);
    if (ret < 0) {
        ERR("bind failed in %d", errno);
        close(s_fd);
        return -1;
    }

    if (TCP_PROTOCOL & socket_info->_state) {
        ret = listen(s_fd, NetIO::LISTEN_BACKLOG);
        if (ret < 0) {
            ERR("listen failed in %d", errno);
            close(s_fd);
            return -1;
        }
    }
    socket_info->_socket_fd = s_fd;

    if (NULL != m_epoll) {
        m_epoll->AddFd(s_fd, EPOLLIN | EPOLLERR, net_addr);
    }
    return 0;
}

int32_t NetIO::RawConnect(NetAddr net_addr, SocketInfo* socket_info) {
    int32_t s_fd = socket(AF_INET,
        ((socket_info->_state & TCP_PROTOCOL) ? SOCK_STREAM : SOCK_DGRAM), 0);
    if (s_fd < 0) {
        ERR("socket failed in %d", errno);
        return -1;
    }

    int32_t ret = fcntl(s_fd, F_GETFL);
    ret = ((ret < 0 || false == NetIO::NON_BLOCK)
        ? ret : (fcntl(s_fd, F_SETFL, ret | O_NONBLOCK)));
    if (ret < 0) {
        ERR("socket set opt failed in %d", errno);
        close(s_fd);
        return -1;
    }

    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = socket_info->_ip;
    addr.sin_port = socket_info->_port;
    ret = connect(s_fd, reinterpret_cast<struct sockaddr*>(&addr), addrlen);
    if (ret < 0 && errno != EINPROGRESS) {
        ERR("connect failed in %d", errno);
        close(s_fd);
        return -1;
    }

    socket_info->_socket_fd = s_fd;
    if (NULL != m_epoll) {
        uint32_t events = EPOLLERR | (ret < 0 ? EPOLLOUT : EPOLLIN);
        m_epoll->AddFd(s_fd, events, net_addr);
        if (ret < 0) {
            socket_info->_state |= IN_BLOCKED;
        } else {
            socket_info->_state &= (~IN_BLOCKED);
        }
    }

    if (socket_info->_state & TCP_PROTOCOL) {
        socket_info->_addr_info = AUTO_RECONNECT;
    }
    return 0;
}

int32_t NetIO::RawClose(SocketInfo* socket_info) {
    int32_t ret = 0;
    if (socket_info->_socket_fd >= 0) {
        ret = close(socket_info->_socket_fd);
        socket_info->_socket_fd = -1;
    }
    return ret;
}

int32_t NetIO::OnEvent(NetAddr net_addr, uint32_t events) {
    SocketInfo* socket_info = RawGetSocketInfo(net_addr);
    if (NULL == socket_info || 0 == socket_info->_state) {
        return -1;
    }
    if ((EPOLLOUT & events) && m_epoll != NULL) {
        socket_info->_state &= (~IN_BLOCKED);
        m_epoll->ModFd(socket_info->_socket_fd, EPOLLIN | EPOLLERR, net_addr);
    }
    if (EPOLLERR & events) {
        RawClose(socket_info);
        if (socket_info->_state == (CONNECT_ADDR | TCP_PROTOCOL)) {
            if (socket_info->_addr_info > 0) {
                --socket_info->_addr_info;
                RawConnect(net_addr, socket_info);
            }
        }
    }
    return 0;
}

}  // namespace xlib
